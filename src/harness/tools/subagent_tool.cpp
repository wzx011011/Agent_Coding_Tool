#include "harness/tools/subagent_tool.h"

#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

SubagentTool::SubagentTool(act::framework::SubagentManager &manager)
    : m_manager(manager)
{
}

QString SubagentTool::name() const
{
    return QStringLiteral("run_subagent");
}

QString SubagentTool::description() const
{
    return QStringLiteral("Spawn a sub-agent to perform a task. "
                          "Sub-agents run in isolation with restricted tools "
                          "and return a structured summary.");
}

QJsonObject SubagentTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("type")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("enum")] =
            QJsonArray{QStringLiteral("explore"), QStringLiteral("code")};
        obj[QStringLiteral("description")] =
            QStringLiteral("The type of sub-agent: "
                           "'explore' for read-only tasks, "
                           "'code' for implementation tasks");
        return obj;
    }();

    props[QStringLiteral("task")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("The task description for the sub-agent");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("type"), QStringLiteral("task")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult SubagentTool::execute(const QJsonObject &params)
{
    auto typeStr = params.value(QStringLiteral("type")).toString();
    auto task = params.value(QStringLiteral("task")).toString();

    if (typeStr.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("run_subagent requires 'type' parameter "
                           "(explore or code)"));
    }

    if (task.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("run_subagent requires 'task' parameter"));
    }

    auto subagentType = parseSubagentType(typeStr);
    if (!subagentType)
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Invalid sub-agent type: '%1'. "
                           "Must be 'explore' or 'code'")
                .arg(typeStr));
    }

    act::framework::SubagentConfig config;
    config.type = *subagentType;
    config.task = task;

    auto subagentId = m_manager.spawn(config);

    spdlog::info("SubagentTool: spawned {} sub-agent '{}' for task: '{}'",
                 typeStr.toStdString(),
                 subagentId.toStdString(),
                 task.toStdString());

    QJsonObject meta;
    meta[QStringLiteral("subagent_id")] = subagentId;
    meta[QStringLiteral("type")] = typeStr;

    return act::core::ToolResult::ok(
        QStringLiteral("Spawned %1 sub-agent (id: %2) for task: %3")
            .arg(typeStr, subagentId, task),
        meta);
}

act::core::PermissionLevel SubagentTool::permissionLevel() const
{
    return act::core::PermissionLevel::Exec;
}

bool SubagentTool::isThreadSafe() const
{
    return false;
}

std::optional<act::framework::SubagentType>
SubagentTool::parseSubagentType(const QString &str)
{
    if (str == QLatin1String("explore"))
        return act::framework::SubagentType::Explore;
    if (str == QLatin1String("code"))
        return act::framework::SubagentType::Code;
    return std::nullopt;
}

} // namespace act::harness
