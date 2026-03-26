#pragma once

#include <QObject>
#include <QString>

namespace act::framework
{

class InteractiveSessionController;

/// Abstract interface for a bidirectional messaging channel.
/// A channel represents a communication path through which users
/// interact with the agent (e.g., Feishu, stdin, future platforms).
class IChannel : public QObject
{
    Q_OBJECT

public:
    explicit IChannel(const QString &channelId, QObject *parent = nullptr);
    virtual ~IChannel() = default;

    [[nodiscard]] QString channelId() const;

    /// Human-readable name for display in logs and status.
    [[nodiscard]] virtual QString displayName() const = 0;

    /// Start the channel (connect, bind, etc.).
    virtual void start() = 0;

    /// Stop the channel gracefully.
    virtual void stop() = 0;

    /// Whether the channel is currently active.
    [[nodiscard]] virtual bool isActive() const = 0;

signals:
    /// A user message was received from this channel.
    /// `conversationId` identifies the conversation thread (e.g., chat_id).
    /// `senderId` identifies the user who sent the message.
    void messageReceived(const QString &conversationId,
                         const QString &senderId,
                         const QString &text);

    /// The channel should send a response to the given conversation.
    void sendRequested(const QString &conversationId,
                       const QString &text);

    /// Channel status changed (connected, disconnected, error).
    void statusChanged(const QString &message);

    /// A new AI session was created for the given conversation.
    /// Observers connect to the controller for streaming token display.
    void sessionCreated(const QString &conversationId,
                        InteractiveSessionController *controller);

protected:
    QString m_channelId;
};

} // namespace act::framework
