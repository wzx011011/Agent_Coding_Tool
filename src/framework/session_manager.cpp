#include "framework/session_manager.h"

#include "framework/agent_loop.h"

#include <spdlog/spdlog.h>

#include <thread>

namespace act::framework
{

ConversationSessionManager::ConversationSessionManager(
    act::services::AIEngine &engine,
    act::harness::ToolRegistry &tools,
    act::harness::PermissionManager &permissions,
    act::harness::ContextManager &contextTemplate,
    QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_tools(tools)
    , m_permissions(permissions)
    , m_contextTemplate(contextTemplate)
{
}

ConversationSessionManager::~ConversationSessionManager()
{
    // Let running threads finish naturally — they hold references via capture
}

int ConversationSessionManager::activeSessionCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_sessions.size();
}

void ConversationSessionManager::submitMessage(
    const QString &conversationId,
    const QString &channelId,
    const QString &senderId,
    const QString &text)
{
    ConversationSession session;

    {
        QMutexLocker locker(&m_mutex);
        if (m_sessions.contains(conversationId))
        {
            session = m_sessions.value(conversationId);
            if (session.busy)
            {
                spdlog::warn("SessionManager: session {} is busy, dropping message",
                             conversationId.toStdString());
                emit sessionError(conversationId,
                                 QStringLiteral("SESSION_BUSY"),
                                 QStringLiteral("Session is processing another request"));
                return;
            }
        }
        session.lastActivity = QDateTime::currentDateTime();
        session.busy = true;
        m_sessions[conversationId] = session;
    }

    // Run agent in a detached thread so the main thread stays responsive
    // Capture channelId and conversationId by value for thread safety
    std::thread([this, conversationId, channelId, text]() {
        runAgentForSession(conversationId, channelId, text);
    }).detach();
}

void ConversationSessionManager::runAgentForSession(
    const QString &conversationId,
    const QString &channelId,
    const QString &text)
{
    // Get session messages
    QList<act::core::LLMMessage> messages;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_sessions.contains(conversationId))
            return;
        messages = m_sessions[conversationId].messages;
    }

    // Create a detached engine instance for thread safety
    auto workerEngine = m_engine.createDetachedInstance();

    // Build a fresh PermissionManager with auto-approve for channel sessions
    act::harness::PermissionManager channelPerms;
    channelPerms.setPermissionCallback(
        [](const act::core::PermissionRequest &) { return true; });

    // Create a fresh ContextManager for this session
    act::harness::ContextManager channelContext;

    // Create and run the AgentLoop
    AgentLoop loop(*workerEngine, m_tools, channelPerms, channelContext);
    loop.setMaxTurns(50);
    loop.setSystemPrompt(m_systemPrompt);

    // Collect the final assistant response text
    QString responseText;

    loop.setEventCallback([](const act::core::RuntimeEvent &) {
        // Events are logged but not forwarded to channel (simplification)
        // Future: forward tool call summaries back to Feishu
    });

    loop.submitUserMessage(text);

    // Extract the last assistant message as the response
    const auto &loopMessages = loop.messages();
    for (auto it = loopMessages.crbegin(); it != loopMessages.crend(); ++it)
    {
        if (it->role == act::core::MessageRole::Assistant && !it->content.isEmpty())
        {
            responseText = it->content;
            break;
        }
    }

    if (responseText.isEmpty() && loop.state() != act::core::TaskState::Completed)
    {
        responseText = QStringLiteral("[Agent error: request failed]");
        spdlog::warn("SessionManager: agent loop for {} ended with state {}",
                     conversationId.toStdString(),
                     static_cast<int>(loop.state()));
    }

    // Update session state
    {
        QMutexLocker locker(&m_mutex);
        if (m_sessions.contains(conversationId))
        {
            auto &session = m_sessions[conversationId];
            session.messages = loopMessages;
            session.busy = false;
            session.lastActivity = QDateTime::currentDateTime();
        }
    }

    // Emit response (thread-safe: signal crosses thread boundary via Qt)
    if (!responseText.isEmpty())
    {
        QMetaObject::invokeMethod(this, [this, conversationId, channelId, responseText]() {
            emit responseReady(conversationId, channelId, responseText);
        });
    }
}

void ConversationSessionManager::resetConversation(const QString &conversationId)
{
    QMutexLocker locker(&m_mutex);
    m_sessions.remove(conversationId);
}

void ConversationSessionManager::cleanupIdleSessions(int maxAgeMinutes)
{
    const auto cutoff = QDateTime::currentDateTime().addSecs(-maxAgeMinutes * 60);

    QMutexLocker locker(&m_mutex);
    for (auto it = m_sessions.begin(); it != m_sessions.end();)
    {
        if (it->lastActivity < cutoff && !it->busy)
        {
            spdlog::info("SessionManager: cleaning up idle session {}",
                         it.key().toStdString());
            it = m_sessions.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace act::framework
