#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include <functional>
#include <mutex>

class QProcess;

namespace act::infrastructure {

/// Abstract transport layer for MCP JSON-RPC communication.
/// Concrete implementations handle the wire protocol (stdio, SSE, etc.).
class McpTransport {
public:
    virtual ~McpTransport() = default;

    /// Establish connection to the MCP server.
    /// Returns true on success.
    virtual bool connect() = 0;

    /// Send a JSON-RPC request and return the response.
    /// Blocks until a response is received or timeout expires.
    virtual QJsonObject sendRequest(const QString &method,
                                     const QJsonObject &params) = 0;

    /// Close the connection.
    virtual void close() = 0;

    /// Check if currently connected.
    [[nodiscard]] virtual bool isConnected() const = 0;

    /// Notification handler callback type.
    using NotificationHandler = std::function<void(const QJsonObject &)>;

    /// Set a handler for JSON-RPC notifications (messages without an id).
    void setNotificationHandler(NotificationHandler handler);

protected:
    NotificationHandler m_notificationHandler;
};

/// Stdio-based MCP transport using QProcess.
/// Launches an MCP server as a subprocess and communicates via stdin/stdout
/// using newline-delimited JSON-RPC messages.
class McpStdioTransport : public McpTransport {
public:
    explicit McpStdioTransport(const QString &command,
                                const QStringList &args = {},
                                const QMap<QString, QString> &env = {});
    ~McpStdioTransport() override;

    // Non-copyable, non-movable
    McpStdioTransport(const McpStdioTransport &) = delete;
    McpStdioTransport &operator=(const McpStdioTransport &) = delete;

    bool connect() override;
    QJsonObject sendRequest(const QString &method,
                             const QJsonObject &params) override;
    void close() override;
    [[nodiscard]] bool isConnected() const override;

    /// Default timeout for waiting on a response (5 seconds).
    static constexpr int kDefaultTimeoutMs = 5000;

private:
    void readLoop();
    void writeMessage(const QJsonObject &msg);
    [[nodiscard]] QJsonObject waitForResponse(int id, int timeoutMs);

    QProcess *m_process = nullptr;
    QString m_command;
    QStringList m_args;
    QMap<QString, QString> m_env;
    bool m_connected = false;
    QByteArray m_buffer;

    /// Guards response state shared between the read loop and callers.
    mutable std::mutex m_mutex;

    /// Signaled when a new response arrives.
    std::condition_variable m_cv;

    /// Maps request id to its JSON-RPC response object.
    QMap<int, QJsonObject> m_responseMap;

    /// Monotonically increasing request id counter.
    int m_nextId = 0;
};

} // namespace act::infrastructure
