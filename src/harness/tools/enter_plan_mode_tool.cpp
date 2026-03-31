#include "harness/tools/enter_plan_mode_tool.h"

#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "framework/agent_loop.h"

namespace act::harness
{

EnterPlanModeTool::EnterPlanModeTool(act::framework::AgentLoop &loop)
    : m_loop(loop)
{
}

QString EnterPlanModeTool::name() const
{
    return QStringLiteral("enter_plan_mode");
}

QString EnterPlanModeTool::description() const
{
    return QStringLiteral("Enter Plan Mode. In Plan Mode, only read-only and "
                          "network tools are available. Use this for code "
                          "exploration and analysis without risk of modifications.");
}

QJsonObject EnterPlanModeTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("reason")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Optional reason for entering Plan Mode");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult EnterPlanModeTool::execute(const QJsonObject &params)
{
    if (m_loop.isPlanMode())
    {
        return act::core::ToolResult::ok(
            QStringLiteral("Already in Plan Mode."));
    }

    auto reason = params.value(QStringLiteral("reason")).toString();
    if (!reason.isEmpty())
    {
        spdlog::info("EnterPlanModeTool: reason: {}", reason.toStdString());
    }

    m_loop.enterPlanMode();
    return act::core::ToolResult::ok(
        QStringLiteral("Entered Plan Mode. "
                       "Only Read/Network level tools are available. "
                       "Use exit_plan_mode to return to normal mode."));
}

act::core::PermissionLevel EnterPlanModeTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool EnterPlanModeTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
