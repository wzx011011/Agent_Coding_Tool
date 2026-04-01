#include "harness/mcp_tool_adapter.h"

#include "core/error_codes.h"

#include <spdlog/spdlog.h>

namespace act::harness {

McpToolAdapter::McpToolAdapter(act::infrastructure::McpClient &client,
                                 const QString &toolName,
                                 const QString &description,
                                 const QJsonObject &inputSchema)
    : m_client(client)
    , m_toolName(toolName)
    , m_description(description)
    , m_inputSchema(inputSchema)
{
}

QString McpToolAdapter::name() const
{
    return m_toolName;
}

QString McpToolAdapter::description() const
{
    return m_description;
}

QJsonObject McpToolAdapter::schema() const
{
    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("properties")] =
        m_inputSchema.value(QStringLiteral("properties")).toObject();

    // Copy required fields if present in the MCP schema.
    auto required = m_inputSchema.value(QStringLiteral("required"));
    if (required.isArray())
        schema[QStringLiteral("required")] = required;

    return schema;
}

act::core::ToolResult McpToolAdapter::execute(const QJsonObject &params)
{
    if (!m_client.isInitialized())
    {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("MCP client is not initialized"));
    }

    spdlog::debug("McpToolAdapter: calling tool '{}'", m_toolName.toStdString());

    QJsonObject response = m_client.callTool(m_toolName, params);

    // Check for JSON-RPC level error.
    if (response.contains(QStringLiteral("error")))
    {
        auto err = response.value(QStringLiteral("error")).toObject();
        QString errorMsg = err.value(QStringLiteral("message")).toString();
        int errorCode = err.value(QStringLiteral("code")).toInt();

        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("MCP tool '%1' error (code %2): %3")
                .arg(m_toolName)
                .arg(errorCode)
                .arg(errorMsg));
    }

    // Extract the result.
    auto result = response.value(QStringLiteral("result")).toObject();

    // MCP tools/call result has "content" array with text items.
    auto content = result.value(QStringLiteral("content")).toArray();

    // Check if the tool reported an error via isError field.
    bool isError = result.value(QStringLiteral("isError")).toBool(false);

    QString output;
    for (const auto &item : content)
    {
        auto obj = item.toObject();
        if (obj.value(QStringLiteral("type")).toString() ==
            QStringLiteral("text"))
        {
            if (!output.isEmpty())
                output.append(QLatin1Char('\n'));
            output.append(obj.value(QStringLiteral("text")).toString());
        }
    }

    if (isError)
    {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            output.isEmpty()
                ? QStringLiteral("MCP tool '%1' returned an error")
                      .arg(m_toolName)
                : output);
    }

    QJsonObject metadata;
    metadata[QStringLiteral("tool")] = m_toolName;

    return act::core::ToolResult::ok(
        output.isEmpty() ? QStringLiteral("(no output)") : output,
        metadata);
}

act::core::PermissionLevel McpToolAdapter::permissionLevel() const
{
    // MCP tools are external network-accessed tools.
    return act::core::PermissionLevel::Network;
}

bool McpToolAdapter::isThreadSafe() const
{
    // MCP tool calls go through the client which is mutex-guarded.
    return true;
}

} // namespace act::harness
