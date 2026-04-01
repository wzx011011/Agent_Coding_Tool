#pragma once

#include <QJsonObject>
#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/mcp_client.h"

namespace act::harness {

/// Adapts an MCP tool as an ITool for the ACT agent system.
///
/// Each McpToolAdapter wraps a single tool discovered from an MCP server
/// and delegates execution to the McpClient.
class McpToolAdapter : public ITool {
public:
    /// Construct an adapter for a discovered MCP tool.
    /// @param client  The MCP client to delegate tool calls to.
    /// @param toolName  The tool name on the MCP server.
    /// @param description  Human-readable description.
    /// @param inputSchema  JSON Schema for input parameters.
    explicit McpToolAdapter(act::infrastructure::McpClient &client,
                             const QString &toolName,
                             const QString &description,
                             const QJsonObject &inputSchema);

    ~McpToolAdapter() override = default;

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    act::core::ToolResult execute(const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    act::infrastructure::McpClient &m_client;
    QString m_toolName;
    QString m_description;
    QJsonObject m_inputSchema;
};

} // namespace act::harness
