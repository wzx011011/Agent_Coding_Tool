#include "harness/tools/task_update_tool.h"

#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

TaskUpdateTool::TaskUpdateTool(TaskManager &manager)
    : m_manager(manager)
{
}

QString TaskUpdateTool::name() const
{
    return QStringLiteral("task_update");
}

QString TaskUpdateTool::description() const
{
    return QStringLiteral("Update an existing task's status, description, dependencies, or active form.");
}

QJsonObject TaskUpdateTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("taskId")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("The ID of the task to update");
        return obj;
    }();

    props[QStringLiteral("status")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("enum")] = QJsonArray{
            QStringLiteral("pending"),
            QStringLiteral("in_progress"),
            QStringLiteral("completed"),
            QStringLiteral("deleted")};
        obj[QStringLiteral("description")] =
            QStringLiteral("New status for the task");
        return obj;
    }();

    props[QStringLiteral("description")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Updated description for the task");
        return obj;
    }();

    props[QStringLiteral("addBlockedBy")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("array");
        obj[QStringLiteral("items")] = [] {
            QJsonObject item;
            item[QStringLiteral("type")] = QStringLiteral("string");
            return item;
        }();
        obj[QStringLiteral("description")] =
            QStringLiteral("Task IDs to add as dependencies");
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
        QJsonArray{QStringLiteral("taskId")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult TaskUpdateTool::execute(const QJsonObject &params)
{
    auto taskId = params.value(QStringLiteral("taskId")).toString();
    if (taskId.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("task_update requires 'taskId' parameter"));
    }

    if (!m_manager.hasTask(taskId))
    {
        return act::core::ToolResult::err(
            act::core::errors::TOOL_NOT_FOUND,
            QStringLiteral("Task not found: %1").arg(taskId));
    }

    auto status = params.value(QStringLiteral("status")).toString();
    auto description =
        params.value(QStringLiteral("description")).toString();
    auto activeForm =
        params.value(QStringLiteral("activeForm")).toString();

    QStringList addBlockedBy;
    auto blockedArr = params.value(QStringLiteral("addBlockedBy")).toArray();
    for (const auto &v : blockedArr)
        addBlockedBy.append(v.toString());

    // Validate status enum if provided
    if (!status.isEmpty())
    {
        const QStringList valid = {
            QStringLiteral("pending"),
            QStringLiteral("in_progress"),
            QStringLiteral("completed"),
            QStringLiteral("deleted")};
        if (!valid.contains(status))
        {
            return act::core::ToolResult::err(
                act::core::errors::INVALID_PARAMS,
                QStringLiteral("Invalid status: '%1'. Must be one of: "
                               "pending, in_progress, completed, deleted")
                    .arg(status));
        }
    }

    bool updated = m_manager.updateTask(taskId, status, description,
                                        addBlockedBy, activeForm);
    if (!updated)
    {
        return act::core::ToolResult::err(
            act::core::errors::TOOL_NOT_FOUND,
            QStringLiteral("Failed to update task: %1").arg(taskId));
    }

    spdlog::info("TaskUpdateTool: updated task id={}", taskId.toStdString());

    auto task = m_manager.getTask(taskId);

    QJsonObject meta;
    meta[QStringLiteral("taskId")] = taskId;
    meta[QStringLiteral("status")] = task.status;

    return act::core::ToolResult::ok(
        QStringLiteral("Updated task %1: [%2]").arg(taskId, task.status),
        meta);
}

act::core::PermissionLevel TaskUpdateTool::permissionLevel() const
{
    return act::core::PermissionLevel::Write;
}

bool TaskUpdateTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
