#include "harness/tools/task_create_tool.h"

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

TaskCreateTool::TaskCreateTool(TaskManager &manager)
    : m_manager(manager)
{
}

QString TaskCreateTool::name() const
{
    return QStringLiteral("task_create");
}

QString TaskCreateTool::description() const
{
    return QStringLiteral("Create a new task with a subject and optional description.");
}

QJsonObject TaskCreateTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("subject")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Brief title for the task");
        return obj;
    }();

    props[QStringLiteral("description")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Detailed description of the task");
        return obj;
    }();

    props[QStringLiteral("activeForm")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Present continuous form for spinner (e.g., 'Running tests')");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("subject")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult TaskCreateTool::execute(const QJsonObject &params)
{
    auto subject = params.value(QStringLiteral("subject")).toString();
    if (subject.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("task_create requires 'subject' parameter"));
    }

    auto description =
        params.value(QStringLiteral("description")).toString();
    auto activeForm =
        params.value(QStringLiteral("activeForm")).toString();

    auto id = m_manager.createTask(subject, description, activeForm);

    spdlog::info("TaskCreateTool: created task '{}' id={}",
                 subject.toStdString(), id.toStdString());

    QJsonObject meta;
    meta[QStringLiteral("taskId")] = id;
    meta[QStringLiteral("subject")] = subject;

    return act::core::ToolResult::ok(
        QStringLiteral("Created task %1: %2").arg(id, subject),
        meta);
}

act::core::PermissionLevel TaskCreateTool::permissionLevel() const
{
    return act::core::PermissionLevel::Write;
}

bool TaskCreateTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
