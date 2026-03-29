#pragma once

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>

#include "core/types.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/ai_engine.h"

namespace act::framework
{

/// Per-conversation session state.
struct ConversationSession
{
    QList<act::core::LLMMessage> messages;
    bool busy = false;
    QDateTime lastActivity;
};

/// Manages multiple concurrent conversations from external channels.
/// Each conversation (identified by conversationId / chat_id) gets its own
/// message history and runs the AgentLoop in a worker thread.
class ConversationSessionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConversationSessionManager(
        act::services::AIEngine &engine,
        act::harness::ToolRegistry &tools,
        act::harness::PermissionManager &permissions,
        act::harness::ContextManager &contextTemplate,
        QObject *parent = nullptr);
    ~ConversationSessionManager() override;

    /// Submit a message from a channel into a conversation.
    void submitMessage(const QString &conversationId,
                       const QString &channelId,
                       const QString &senderId,
                       const QString &text);

    /// Get the number of active sessions.
    [[nodiscard]] int activeSessionCount() const;

    /// Reset a specific conversation.
    void resetConversation(const QString &conversationId);

    /// Clean up sessions idle longer than maxAgeMinutes.
    void cleanupIdleSessions(int maxAgeMinutes = 30);

    /// Set the system prompt to inject before the first user message.
    void setSystemPrompt(const QString &prompt) { m_systemPrompt = prompt; }

signals:
    /// A response is ready to be sent back through the channel.
    void responseReady(const QString &conversationId,
                       const QString &channelId,
                       const QString &text);

    /// An error occurred during processing.
    void sessionError(const QString &conversationId,
                      const QString &errorCode,
                      const QString &errorMessage);

private:
    void runAgentForSession(const QString &conversationId,
                            const QString &channelId,
                            const QString &text);

    QMap<QString, ConversationSession> m_sessions;
    mutable QMutex m_mutex;

    act::services::AIEngine &m_engine;
    act::harness::ToolRegistry &m_tools;
    act::harness::PermissionManager &m_permissions;
    act::harness::ContextManager &m_contextTemplate;
    QString m_systemPrompt;
};

} // namespace act::framework
