#include "harness/tools/task_get_tool.h"

#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

TaskGetTool::TaskGetTool(TaskManager &manager)
    : m_manager(manager)
{
}

QString TaskGetTool::name() const
{
    return QStringLiteral("task_get");
}

QString TaskGetTool::description() const
{
    return QStringLiteral("Retrieve a task by its ID.");
}

QJsonObject TaskGetTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("taskId")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("The ID of the task to retrieve");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("taskId")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult TaskGetTool::execute(const QJsonObject &params)
{
    auto taskId = params.value(QStringLiteral("taskId")).toString();
    if (taskId.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("task_get requires 'taskId' parameter"));
    }

    if (!m_manager.hasTask(taskId))
    {
        return act::core::ToolResult::err(
            act::core::errors::TOOL_NOT_FOUND,
            QStringLiteral("Task not found: %1").arg(taskId));
    }

    auto task = m_manager.getTask(taskId);

    QJsonObject taskObj;
    taskObj[QStringLiteral("id")] = task.id;
    taskObj[QStringLiteral("subject")] = task.subject;
    taskObj[QStringLiteral("description")] = task.description;
    taskObj[QStringLiteral("status")] = task.status;
    taskObj[QStringLiteral("owner")] = task.owner;
    taskObj[QStringLiteral("activeForm")] = task.activeForm;
    taskObj[QStringLiteral("createdAt")] = task.createdAt.toString(Qt::ISODate);
    taskObj[QStringLiteral("updatedAt")] = task.updatedAt.toString(Qt::ISODate);

    QJsonArray blockedByArr;
    for (const auto &dep : task.blockedBy)
        blockedByArr.append(dep);
    taskObj[QStringLiteral("blockedBy")] = blockedByArr;

    // Merge any existing metadata
    QJsonObject meta = task.metadata;
    meta[QStringLiteral("task")] = taskObj;

    QString output = QStringLiteral("Task %1: %2 [%3]")
                         .arg(task.id, task.subject, task.status);

    return act::core::ToolResult::ok(output, meta);
}

act::core::PermissionLevel TaskGetTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool TaskGetTool::isThreadSafe() const
{
    return true;
}

} // namespace act::harness
