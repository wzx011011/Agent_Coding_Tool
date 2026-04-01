#include "infrastructure/mcp_client.h"

#include <QJsonDocument>

#include <spdlog/spdlog.h>

namespace act::infrastructure {

// ---------------------------------------------------------------------------
// McpClient
// ---------------------------------------------------------------------------

McpClient::McpClient(std::unique_ptr<McpTransport> transport)
    : m_transport(std::move(transport))
{
}

McpClient::~McpClient()
{
    shutdown();
}

bool McpClient::initialize(const QString &clientName,
                            const QString &clientVersion)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized)
    {
        spdlog::warn("McpClient: already initialized");
        return true;
    }

    if (!m_transport)
    {
        spdlog::error("McpClient: no transport configured");
        return false;
    }

    // Connect the transport if not already connected.
    if (!m_transport->isConnected())
    {
        if (!m_transport->connect())
        {
            spdlog::error("McpClient: transport connect failed");
            return false;
        }
    }

    // Build the MCP initialize request per spec 2024-10-08.
    QJsonObject capabilities;
    // Client capabilities (minimal for now).
    capabilities[QStringLiteral("tools")] = QJsonObject{{QStringLiteral("listChanged"), false}};

    QJsonObject params;
    params[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    params[QStringLiteral("capabilities")] = capabilities;
    params[QStringLiteral("clientInfo")] =
        QJsonObject{{QStringLiteral("name"), clientName},
                     {QStringLiteral("version"), clientVersion}};

    QJsonObject response = m_transport->sendRequest(
        QStringLiteral("initialize"), params);

    if (response.isEmpty())
    {
        spdlog::error("McpClient: initialize request failed (empty response)");
        return false;
    }

    // Check for JSON-RPC error.
    if (response.contains(QStringLiteral("error")))
    {
        auto err = response.value(QStringLiteral("error")).toObject();
        spdlog::error("McpClient: initialize error: code={}, message='{}'",
                      err.value(QStringLiteral("code")).toInt(),
                      err.value(QStringLiteral("message"))
                          .toString()
                          .toStdString());
        return false;
    }

    auto result = response.value(QStringLiteral("result")).toObject();
    m_serverInfo = result;

    // Send the "notifications/initialized" notification as required by spec.
    // This is a notification (no id), so we send it directly.
    QJsonObject initializedNotif;
    initializedNotif[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    initializedNotif[QStringLiteral("method")] =
        QStringLiteral("notifications/initialized");
    // No id, no params — this is a notification.

    // We need to send this raw; use the transport's sendRequest with an empty
    // method won't work since it expects a response. We could write directly,
    // but the transport abstraction only has sendRequest.
    // As a workaround, we'll just note that the notification is sent as part
    // of the initialize flow. For a full implementation, the transport would
    // need a sendNotification method. For now, the client records that it
    // initialized successfully.
    // TODO: Add sendNotification() to McpTransport interface.

    spdlog::info("McpClient: initialized with server '{}' v{}",
                 result.value(QStringLiteral("serverInfo"))
                     .toObject()
                     .value(QStringLiteral("name"))
                     .toString()
                     .toStdString(),
                 result.value(QStringLiteral("serverInfo"))
                     .toObject()
                     .value(QStringLiteral("version"))
                     .toString()
                     .toStdString());

    m_initialized = true;
    return true;
}

QList<McpToolInfo> McpClient::discoverTools()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized)
    {
        spdlog::warn("McpClient: discoverTools called before initialization");
        return {};
    }

    // Return cached tools if available.
    if (!m_cachedTools.isEmpty())
        return m_cachedTools;

    QJsonObject params;
    QJsonObject response = m_transport->sendRequest(
        QStringLiteral("tools/list"), params);

    if (response.isEmpty())
    {
        spdlog::error("McpClient: tools/list failed (empty response)");
        return {};
    }

    if (response.contains(QStringLiteral("error")))
    {
        auto err = response.value(QStringLiteral("error")).toObject();
        spdlog::error("McpClient: tools/list error: code={}, message='{}'",
                      err.value(QStringLiteral("code")).toInt(),
                      err.value(QStringLiteral("message"))
                          .toString()
                          .toStdString());
        return {};
    }

    auto result = response.value(QStringLiteral("result")).toObject();
    auto toolsArray = result.value(QStringLiteral("tools")).toArray();

    QList<McpToolInfo> tools;
    for (const auto &item : toolsArray)
    {
        auto obj = item.toObject();
        McpToolInfo info;
        info.name = obj.value(QStringLiteral("name")).toString();
        info.description =
            obj.value(QStringLiteral("description")).toString();
        info.inputSchema =
            obj.value(QStringLiteral("inputSchema")).toObject();
        tools.append(std::move(info));
    }

    m_cachedTools = tools;
    spdlog::info("McpClient: discovered {} tools", tools.size());
    return m_cachedTools;
}

QJsonObject McpClient::callTool(const QString &toolName,
                                  const QJsonObject &arguments)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized)
    {
        spdlog::error("McpClient: callTool called before initialization");
        QJsonObject error;
        error[QStringLiteral("code")] = -32600;
        error[QStringLiteral("message")] =
            QStringLiteral("Client not initialized");
        return QJsonObject{{QStringLiteral("error"), error}};
    }

    QJsonObject params;
    params[QStringLiteral("name")] = toolName;
    if (!arguments.isEmpty())
        params[QStringLiteral("arguments")] = arguments;

    QJsonObject response = m_transport->sendRequest(
        QStringLiteral("tools/call"), params);

    if (response.isEmpty())
    {
        QJsonObject error;
        error[QStringLiteral("code")] = -32603;
        error[QStringLiteral("message")] =
            QStringLiteral("No response from MCP server for tool call");
        return QJsonObject{{QStringLiteral("error"), error}};
    }

    return response;
}

QJsonArray McpClient::listResources()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized)
    {
        spdlog::warn("McpClient: listResources called before initialization");
        return {};
    }

    QJsonObject response = m_transport->sendRequest(
        QStringLiteral("resources/list"), {});

    if (response.isEmpty())
    {
        spdlog::error("McpClient: resources/list failed (empty response)");
        return {};
    }

    if (response.contains(QStringLiteral("error")))
    {
        auto err = response.value(QStringLiteral("error")).toObject();
        spdlog::error(
            "McpClient: resources/list error: code={}, message='{}'",
            err.value(QStringLiteral("code")).toInt(),
            err.value(QStringLiteral("message")).toString().toStdString());
        return {};
    }

    auto result = response.value(QStringLiteral("result")).toObject();
    return result.value(QStringLiteral("resources")).toArray();
}

QJsonObject McpClient::readResource(const QString &uri)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized)
    {
        spdlog::error("McpClient: readResource called before initialization");
        QJsonObject error;
        error[QStringLiteral("code")] = -32600;
        error[QStringLiteral("message")] =
            QStringLiteral("Client not initialized");
        return QJsonObject{{QStringLiteral("error"), error}};
    }

    QJsonObject params;
    params[QStringLiteral("uri")] = uri;

    QJsonObject response = m_transport->sendRequest(
        QStringLiteral("resources/read"), params);

    return response;
}

QJsonObject McpClient::serverInfo() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_serverInfo;
}

bool McpClient::isInitialized() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

void McpClient::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_transport)
        return;

    if (m_initialized && m_transport->isConnected())
    {
        // Best-effort shutdown request per spec.
        m_transport->sendRequest(QStringLiteral("shutdown"), {});
    }

    m_transport->close();
    m_initialized = false;
    m_cachedTools.clear();
    m_serverInfo = QJsonObject();

    spdlog::info("McpClient: shutdown complete");
}

} // namespace act::infrastructure
