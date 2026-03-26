#pragma once

#include <QMap>
#include <QObject>
#include <QSet>
#include <memory>

#include "framework/channel.h"
#include "infrastructure/feishu_rest_client.h"
#include "infrastructure/feishu_ws_client.h"

namespace act::harness
{
class PermissionManager;
}

namespace act::services
{
class AIEngine;
}

namespace act::harness
{
class ToolRegistry;
class ContextManager;
}

namespace act::framework
{

class InteractiveSessionController;

/// Feishu messaging channel adapter.
/// Bridges Feishu WebSocket/REST clients to the IChannel interface.
class FeishuChannel : public IChannel
{
    Q_OBJECT

public:
    struct AIDeps
    {
        act::services::AIEngine *engine = nullptr;
        act::harness::ToolRegistry *tools = nullptr;
        act::harness::ContextManager *context = nullptr;
    };

    struct Config
    {
        QString appId;
        QString appSecret;
        QString proxyHost;
        int proxyPort = 0;
        int timeoutSeconds = 30;
        AIDeps ai;
    };

    explicit FeishuChannel(const Config &config, QObject *parent = nullptr);
    ~FeishuChannel() override;

    [[nodiscard]] QString displayName() const override;
    void start() override;
    void stop() override;
    [[nodiscard]] bool isActive() const override;

    [[nodiscard]] int activeSessionCount() const;
    void setSessionTimeoutMinutes(int minutes);
    void cleanupIdleSessions();

private slots:
    void onEventReceived(const act::infrastructure::feishu::FeishuEvent &event);
    void onWsConnectionChanged(bool connected);
    void onWsError(const QString &code, const QString &msg);

private:
    void handleReceivedMessage(const act::infrastructure::feishu::FeishuMessage &msg);
    void handleAIResponse(const QString &chatId);
    std::shared_ptr<InteractiveSessionController> getOrCreateSession(const QString &chatId);

    struct QueuedMessage
    {
        QString messageId;
        QString content;
    };

    Config m_config;
    act::infrastructure::FeishuRestClient *m_restClient;
    act::infrastructure::FeishuWsClient *m_wsClient;
    bool m_active = false;

    // Dedicated permission manager with all levels auto-approved
    // (avoids callback conflict with CLI REPL's PermissionManager)
    std::unique_ptr<act::harness::PermissionManager> m_feishuPermissions;

    // Per-chat AI session management
    QMap<QString, std::shared_ptr<InteractiveSessionController>> m_sessions;
    QMap<QString, bool> m_sessionBusy;
    QMap<QString, QString> m_pendingReplyMessageId;
    QMap<QString, QList<QueuedMessage>> m_pendingMessages;

    // Deduplication: track recently processed message IDs to avoid
    // processing retransmitted WS events multiple times.
    QSet<QString> m_processedMessageIds;
};

} // namespace act::framework
