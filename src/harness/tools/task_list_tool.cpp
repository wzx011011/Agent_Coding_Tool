#include "harness/tools/task_list_tool.h"

#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

TaskListTool::TaskListTool(TaskManager &manager)
    : m_manager(manager)
{
}

QString TaskListTool::name() const
{
    return QStringLiteral("task_list");
}

QString TaskListTool::description() const
{
    return QStringLiteral("List tasks, optionally filtered by status.");
}

QJsonObject TaskListTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("status")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("enum")] = QJsonArray{
            QStringLiteral("pending"),
            QStringLiteral("in_progress"),
            QStringLiteral("completed")};
        obj[QStringLiteral("description")] =
            QStringLiteral("Filter tasks by status. Omit to list all non-deleted tasks.");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult TaskListTool::execute(const QJsonObject &params)
{
    auto statusFilter = params.value(QStringLiteral("status")).toString();

    auto tasks = m_manager.listTasks(statusFilter);

    if (tasks.isEmpty())
    {
        QString msg = statusFilter.isEmpty()
                          ? QStringLiteral("No tasks found.")
                          : QStringLiteral("No tasks with status '%1'.").arg(statusFilter);
        QJsonObject meta;
        meta[QStringLiteral("count")] = 0;
        return act::core::ToolResult::ok(msg, meta);
    }

    // Build output lines
    QStringList lines;
    lines.reserve(tasks.size() + 1);
    lines.append(QStringLiteral("Tasks (%1):").arg(tasks.size()));

    for (const auto &task : tasks)
    {
        QString marker;
        if (task.status == QStringLiteral("pending"))
            marker = QStringLiteral("[ ]");
        else if (task.status == QStringLiteral("in_progress"))
            marker = QStringLiteral("[~]");
        else if (task.status == QStringLiteral("completed"))
            marker = QStringLiteral("[x]");
        else
            marker = QStringLiteral("[-]");

        QString line = QStringLiteral("%1 #%2 %3")
                           .arg(marker, task.id, task.subject);

        if (!task.description.isEmpty())
            line += QStringLiteral(" -- %1").arg(task.description);

        lines.append(line);
    }

    QString output = lines.join(QLatin1Char('\n'));

    QJsonObject meta;
    meta[QStringLiteral("count")] = tasks.size();

    return act::core::ToolResult::ok(output, meta);
}

act::core::PermissionLevel TaskListTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool TaskListTool::isThreadSafe() const
{
    return true;
}

} // namespace act::harness
