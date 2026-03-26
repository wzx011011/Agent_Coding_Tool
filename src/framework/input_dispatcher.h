#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <memory>

#include "framework/channel.h"
#include "framework/cli_repl.h"
#include "framework/session_manager.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/ai_engine.h"

namespace act::framework
{

/// Dispatches input from multiple sources (stdin + channels) to the agent.
/// Routes stdin input through CliRepl (synchronous) and channel input
/// through ConversationSessionManager (asynchronous).
class InputDispatcher : public QObject
{
    Q_OBJECT

public:
    InputDispatcher(
        act::services::AIEngine &engine,
        act::harness::ToolRegistry &tools,
        act::harness::PermissionManager &permissions,
        act::harness::ContextManager &context,
        QObject *parent = nullptr);

    /// Register a channel.
    void addChannel(std::shared_ptr<IChannel> channel);

    /// Process input from stdin (delegates to CliRepl).
    [[nodiscard]] act::core::TaskState processStdinInput(const QString &input);

    /// Start all registered channels.
    void startChannels();

    /// Stop all registered channels.
    void stopChannels();

    /// Access the underlying CLI REPL (for slash commands, etc.).
    [[nodiscard]] CliRepl &cliRepl() { return m_cliRepl; }

    /// Get registered channels.
    [[nodiscard]] const QList<std::shared_ptr<IChannel>> &channels() const { return m_channels; }

signals:
    /// A line of output should be displayed.
    void outputLine(const QString &line);

    /// Exit was requested.
    void exitRequested();

private slots:
    void onChannelMessage(const QString &conversationId,
                          const QString &senderId,
                          const QString &text);
    void onSessionResponse(const QString &conversationId,
                           const QString &channelId,
                           const QString &text);
    void onChannelStatusChanged(const QString &message);

private:
    void connectChannel(const std::shared_ptr<IChannel> &channel);

    QList<std::shared_ptr<IChannel>> m_channels;
    QMap<QString, std::shared_ptr<IChannel>> m_channelMap;  // channelId → channel
    ConversationSessionManager m_sessionManager;
    CliRepl m_cliRepl;
};

} // namespace act::framework
