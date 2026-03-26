#include <QTimer>

#include "framework/feishu_channel.h"

#include "framework/interactive_session_controller.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/ai_engine.h"

#include <spdlog/spdlog.h>

namespace act::framework
{

FeishuChannel::FeishuChannel(const Config &config, QObject *parent)
    : IChannel(QStringLiteral("feishu"), parent)
    , m_config(config)
    , m_restClient(new act::infrastructure::FeishuRestClient(this))
    , m_wsClient(new act::infrastructure::FeishuWsClient(m_restClient, this))
{
    m_restClient->setAppCredentials(m_config.appId, m_config.appSecret);
    if (!m_config.proxyHost.isEmpty() && m_config.proxyPort > 0)
        m_restClient->setProxy(m_config.proxyHost, m_config.proxyPort);
    m_restClient->setTimeoutSeconds(m_config.timeoutSeconds);

    // Dedicated permission manager for Feishu: all levels auto-approved
    // (avoids callback conflict with CLI REPL's shared PermissionManager)
    m_feishuPermissions = std::make_unique<act::harness::PermissionManager>();
    m_feishuPermissions->setAutoApproved(act::core::PermissionLevel::Read, true);
    m_feishuPermissions->setAutoApproved(act::core::PermissionLevel::Write, true);
    m_feishuPermissions->setAutoApproved(act::core::PermissionLevel::Exec, true);
    m_feishuPermissions->setAutoApproved(act::core::PermissionLevel::Network, true);
    m_feishuPermissions->setAutoApproved(act::core::PermissionLevel::Destructive, true);

    connect(m_wsClient, &act::infrastructure::FeishuWsClient::eventReceived,
            this, &FeishuChannel::onEventReceived);
    connect(m_wsClient, &act::infrastructure::FeishuWsClient::connectionStateChanged,
            this, &FeishuChannel::onWsConnectionChanged);
    connect(m_wsClient, &act::infrastructure::FeishuWsClient::errorOccurred,
            this, &FeishuChannel::onWsError);

    // Wire sendRequested to REST client
    connect(this, &FeishuChannel::sendRequested,
            this, [this](const QString &chatId, const QString &text) {
                auto resp = m_restClient->sendTextMessage(chatId, text);
                if (!resp.success)
                {
                    spdlog::error("FeishuChannel: failed to send response to {}: [{}] {}",
                                  chatId.toStdString(), resp.code.toStdString(),
                                  resp.msg.toStdString());
                }
            });
}

FeishuChannel::~FeishuChannel()
{
    stop();
}

QString FeishuChannel::displayName() const
{
    return QStringLiteral("Feishu");
}

void FeishuChannel::start()
{
    spdlog::info("FeishuChannel: starting");
    m_wsClient->start();
    m_active = true;
}

void FeishuChannel::stop()
{
    spdlog::info("FeishuChannel: stopping");
    m_wsClient->stop();

    // Shutdown all AI sessions (shared_ptr destructors join worker threads)
    m_sessions.clear();
    m_pendingMessages.clear();
    m_pendingReplyMessageId.clear();
    m_sessionBusy.clear();

    m_active = false;
}

bool FeishuChannel::isActive() const
{
    return m_active;
}

int FeishuChannel::activeSessionCount() const
{
    return m_sessions.size();
}

void FeishuChannel::setSessionTimeoutMinutes(int minutes)
{
    // TODO: implement idle session cleanup timer
    (void)minutes;
}

void FeishuChannel::cleanupIdleSessions()
{
    // TODO: implement idle session cleanup
}

std::shared_ptr<InteractiveSessionController> FeishuChannel::getOrCreateSession(const QString &chatId)
{
    auto it = m_sessions.find(chatId);
    if (it != m_sessions.end())
        return it.value();

    if (!m_config.ai.engine || !m_config.ai.tools || !m_config.ai.context)
    {
        spdlog::error("FeishuChannel: AI dependencies not configured");
        return nullptr;
    }

    auto controller = std::make_shared<InteractiveSessionController>(
        *m_config.ai.engine,
        *m_config.ai.tools,
        *m_feishuPermissions,
        *m_config.ai.context,
        InteractiveSessionExecutionMode::AsyncThreaded);

    // Monitor state changes for response delivery
    QObject::connect(controller.get(), &InteractiveSessionController::stateChanged,
                     this, [this, chatId]() { handleAIResponse(chatId); });

    emit sessionCreated(chatId, controller.get());

    m_sessions.insert(chatId, controller);
    m_sessionBusy[chatId] = false;
    spdlog::info("FeishuChannel: created AI session for chat {}", chatId.toStdString());
    return controller;
}

void FeishuChannel::onEventReceived(const act::infrastructure::feishu::FeishuEvent &event)
{
    auto msg = act::infrastructure::feishu::extractMessage(event);
    if (msg)
    {
        handleReceivedMessage(*msg);
    }
}

void FeishuChannel::handleReceivedMessage(const act::infrastructure::feishu::FeishuMessage &msg)
{
    if (msg.content.isEmpty())
        return;

    // Deduplicate: skip if this messageId was already processed
    if (m_processedMessageIds.contains(msg.messageId))
    {
        spdlog::warn("FeishuChannel: duplicate message {}, skipping",
                     msg.messageId.toStdString());
        return;
    }
    m_processedMessageIds.insert(msg.messageId);

    // Keep the set bounded (retain last 1000 message IDs)
    if (m_processedMessageIds.size() > 1000)
    {
        auto it = m_processedMessageIds.begin();
        m_processedMessageIds.erase(it);
    }

    spdlog::info("FeishuChannel: message from {} in chat {}: {}",
                 msg.senderId.toStdString(),
                 msg.chatId.toStdString(),
                 msg.content.left(80).toStdString());

    emit messageReceived(msg.chatId, msg.senderId, msg.content);

    auto session = getOrCreateSession(msg.chatId);
    if (!session)
        return;

    m_pendingReplyMessageId[msg.chatId] = msg.messageId;

    if (session->isBusy())
    {
        m_pendingMessages[msg.chatId].append({msg.messageId, msg.content});
        spdlog::info("FeishuChannel: queued message for chat {} (busy)",
                     msg.chatId.toStdString());
        return;
    }

    session->submitInput(msg.content);
}

void FeishuChannel::handleAIResponse(const QString &chatId)
{
    auto it = m_sessions.find(chatId);
    if (it == m_sessions.end())
        return;

    auto &ctrl = it.value();
    const auto state = ctrl->snapshotState();
    const bool nowBusy = state.isBusy();
    const bool wasBusy = m_sessionBusy.value(chatId, false);

    m_sessionBusy[chatId] = nowBusy;

    // Only process when transitioning from busy to idle
    if (!wasBusy || nowBusy)
        return;

    // Extract last assistant message
    const auto &messages = state.messages();
    QString responseText;
    for (int i = messages.size() - 1; i >= 0; --i)
    {
        if (messages[i].kind == SessionMessageKind::Assistant && !messages[i].content.isEmpty())
        {
            responseText = messages[i].content;
            break;
        }
    }

    // Defer reply delivery to the next event loop iteration so that
    // any remaining tokenStreamed / turnCompleted events are processed first.
    QTimer::singleShot(0, this, [this, chatId, responseText]() {
        if (!responseText.isEmpty())
        {
            const auto messageId = m_pendingReplyMessageId.value(chatId);
            if (!messageId.isEmpty())
            {
                spdlog::info("FeishuChannel: sending reply to chat {} ({} chars)",
                             chatId.toStdString(), responseText.size());

                auto resp = m_restClient->replyTextMessage(messageId, responseText);
                if (!resp.success)
                {
                    spdlog::error("FeishuChannel: reply failed: [{}] {}",
                                  resp.code.toStdString(), resp.msg.toStdString());
                }
            }
            else
            {
                spdlog::warn("FeishuChannel: no pending messageId for chat {}",
                             chatId.toStdString());
            }
        }
        else
        {
            spdlog::warn("FeishuChannel: no assistant response for chat {}",
                         chatId.toStdString());
        }

        // Process queued messages after reply is sent
        auto pendingIt = m_pendingMessages.find(chatId);
        if (pendingIt != m_pendingMessages.end() && !pendingIt->isEmpty())
        {
            auto sessionIt = m_sessions.find(chatId);
            if (sessionIt == m_sessions.end())
                return;

            const auto next = pendingIt->takeFirst();
            m_pendingReplyMessageId[chatId] = next.messageId;
            if (pendingIt->isEmpty())
                m_pendingMessages.erase(pendingIt);

            spdlog::info("FeishuChannel: processing queued message for chat {}",
                         chatId.toStdString());
            sessionIt.value()->submitInput(next.content);
        }
    });
}

void FeishuChannel::onWsConnectionChanged(bool connected)
{
    if (connected)
    {
        spdlog::info("FeishuChannel: WebSocket connected");
        emit statusChanged(QStringLiteral("Feishu connected"));
    }
    else
    {
        spdlog::warn("FeishuChannel: WebSocket disconnected");
        emit statusChanged(QStringLiteral("Feishu disconnected"));
    }
}

void FeishuChannel::onWsError(const QString &code, const QString &msg)
{
    spdlog::error("FeishuChannel: WebSocket error [{}]: {}", code.toStdString(), msg.toStdString());
    emit statusChanged(QStringLiteral("Feishu error: %1").arg(msg));
}

} // namespace act::framework
