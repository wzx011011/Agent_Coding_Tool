#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include <memory>
#include <mutex>

#include "infrastructure/mcp_transport.h"

namespace act::infrastructure {

/// Configuration for connecting to an MCP server.
struct McpServerConfig {
    QString name;
    QString command;                  // For stdio transport
    QStringList args;                 // For stdio transport
    QMap<QString, QString> env;       // Extra environment variables
    QString url;                      // For SSE transport
    QMap<QString, QString> headers;   // For SSE transport

    enum class TransportType { Stdio, Sse };
    TransportType transportType = TransportType::Stdio;
};

/// Discovered tool metadata from an MCP server.
struct McpToolInfo {
    QString name;
    QString description;
    QJsonObject inputSchema;  // JSON Schema for the tool's input parameters
};

/// MCP client implementing the Model Context Protocol (2024-10-08 spec).
///
/// Handles JSON-RPC 2.0 communication with an MCP server:
/// - initialize / initialized handshake
/// - tools/list and tools/call
/// - resources/list and resources/read
/// - shutdown
class McpClient {
public:
    /// Construct with an explicit transport (for testing with mocks).
    explicit McpClient(std::unique_ptr<McpTransport> transport);

    ~McpClient();

    // Non-copyable
    McpClient(const McpClient &) = delete;
    McpClient &operator=(const McpClient &) = delete;

    /// Perform the MCP initialize / initialized handshake.
    /// Returns true if the server accepted the connection.
    bool initialize(const QString &clientName = QStringLiteral("ACT Agent"),
                    const QString &clientVersion = QStringLiteral("1.0"));

    /// Query the server for available tools.
    /// Returns the list of discovered tools. Results are cached after the
    /// first successful call.
    QList<McpToolInfo> discoverTools();

    /// Invoke a tool by name on the MCP server.
    /// Returns the tool result object (or an error object).
    QJsonObject callTool(const QString &toolName,
                          const QJsonObject &arguments);

    /// List available resources from the server.
    QJsonArray listResources();

    /// Read a specific resource by URI.
    QJsonObject readResource(const QString &uri);

    /// Get server capabilities / info (valid after initialize()).
    [[nodiscard]] QJsonObject serverInfo() const;

    /// Check whether the client has completed the initialize handshake.
    [[nodiscard]] bool isInitialized() const;

    /// Send a shutdown request and close the transport.
    void shutdown();

private:
    QJsonObject sendJsonRpc(const QString &method,
                             const QJsonObject &params);

    void handleNotification(const QJsonObject &notification);

    std::unique_ptr<McpTransport> m_transport;
    bool m_initialized = false;
    QJsonObject m_serverInfo;
    QList<McpToolInfo> m_cachedTools;
    mutable std::mutex m_mutex;
    int m_nextRequestId = 1;
};

} // namespace act::infrastructure
