#include "framework/input_dispatcher.h"

#include <spdlog/spdlog.h>

namespace act::framework
{

InputDispatcher::InputDispatcher(
    act::services::AIEngine &engine,
    act::harness::ToolRegistry &tools,
    act::harness::PermissionManager &permissions,
    act::harness::ContextManager &context,
    QObject *parent)
    : QObject(parent)
    , m_sessionManager(engine, tools, permissions, context, this)
    , m_cliRepl(engine, tools, permissions, context)
{
    // Forward CLI REPL output signals
    connect(&m_cliRepl, &CliRepl::outputLine,
            this, &InputDispatcher::outputLine);
    connect(&m_cliRepl, &CliRepl::exitRequested,
            this, &InputDispatcher::exitRequested);

    // Forward session manager response signal
    connect(&m_sessionManager, &ConversationSessionManager::responseReady,
            this, &InputDispatcher::onSessionResponse);
    connect(&m_sessionManager, &ConversationSessionManager::sessionError,
            this, [](const QString &convId, const QString &code, const QString &msg) {
                spdlog::error("InputDispatcher: session {} error [{}]: {}",
                              convId.toStdString(), code.toStdString(), msg.toStdString());
            });
}

void InputDispatcher::addChannel(std::shared_ptr<IChannel> channel)
{
    if (!channel)
        return;

    // Channel message → session manager
    connect(channel.get(), &IChannel::messageReceived,
            this, &InputDispatcher::onChannelMessage);

    // Channel status → output
    connect(channel.get(), &IChannel::statusChanged,
            this, &InputDispatcher::onChannelStatusChanged);

    // Session response → channel sendRequested
    connect(&m_sessionManager, &ConversationSessionManager::responseReady,
            channel.get(), [channel](const QString &conversationId,
                                     const QString &channelId,
                                     const QString &text) {
                // Only forward if this response is for this channel
                if (channelId == channel->channelId() && channel->isActive())
                {
                    emit channel->sendRequested(conversationId, text);
                }
            });

    m_channels.append(channel);
    m_channelMap[channel->channelId()] = channel;
}

act::core::TaskState InputDispatcher::processStdinInput(const QString &input)
{
    return m_cliRepl.processInput(input);
}

void InputDispatcher::startChannels()
{
    for (const auto &channel : m_channels)
    {
        spdlog::info("InputDispatcher: starting channel '{}'",
                     channel->displayName().toStdString());
        channel->start();
    }
}

void InputDispatcher::stopChannels()
{
    for (const auto &channel : m_channels)
    {
        channel->stop();
    }
}

void InputDispatcher::onChannelMessage(
    const QString &conversationId,
    const QString &senderId,
    const QString &text)
{
    // Identify the channel from the signal sender
    auto *channel = qobject_cast<IChannel *>(sender());
    if (!channel)
    {
        spdlog::warn("InputDispatcher: received message from unknown sender");
        return;
    }

    spdlog::info("InputDispatcher: message from channel '{}' conversation '{}': {}",
                 channel->channelId().toStdString(),
                 conversationId.toStdString(),
                 text.left(80).toStdString());

    m_sessionManager.submitMessage(conversationId, channel->channelId(), senderId, text);
}

void InputDispatcher::onSessionResponse(
    const QString &conversationId,
    const QString &channelId,
    const QString &text)
{
    // This slot receives ALL session responses.
    // Per-channel routing is handled by the direct connection in addChannel().
    // This slot serves as a logging/monitoring hook.
    spdlog::info("InputDispatcher: response ready for conversation '{}': {} chars",
                 conversationId.toStdString(), text.length());
}

void InputDispatcher::onChannelStatusChanged(const QString &message)
{
    emit outputLine(message);
}

} // namespace act::framework
