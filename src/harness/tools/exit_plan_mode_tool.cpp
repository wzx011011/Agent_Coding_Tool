#include "harness/tools/exit_plan_mode_tool.h"

#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "framework/agent_loop.h"

namespace act::harness
{

ExitPlanModeTool::ExitPlanModeTool(act::framework::AgentLoop &loop)
    : m_loop(loop)
{
}

QString ExitPlanModeTool::name() const
{
    return QStringLiteral("exit_plan_mode");
}

QString ExitPlanModeTool::description() const
{
    return QStringLiteral("Exit Plan Mode and restore full tool access. "
                          "Use this when you have finished analysis and are "
                          "ready to make modifications.");
}

QJsonObject ExitPlanModeTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("reason")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Optional reason for exiting Plan Mode");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult ExitPlanModeTool::execute(const QJsonObject &params)
{
    if (!m_loop.isPlanMode())
    {
        return act::core::ToolResult::ok(
            QStringLiteral("Not in Plan Mode."));
    }

    auto reason = params.value(QStringLiteral("reason")).toString();
    if (!reason.isEmpty())
    {
        spdlog::info("ExitPlanModeTool: reason: {}", reason.toStdString());
    }

    m_loop.exitPlanMode();
    return act::core::ToolResult::ok(
        QStringLiteral("Exited Plan Mode. All tools are now available."));
}

act::core::PermissionLevel ExitPlanModeTool::permissionLevel() const
{
    // Write level: exiting plan mode restores access to Write/Exec/Destructive
    // tools, which is a privilege escalation that requires user confirmation.
    return act::core::PermissionLevel::Write;
}

bool ExitPlanModeTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
