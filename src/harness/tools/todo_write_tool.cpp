#include "harness/tools/todo_write_tool.h"

#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

QString TodoWriteTool::name() const
{
    return QStringLiteral("todo_write");
}

QString TodoWriteTool::description() const
{
    return QStringLiteral("Manage an in-memory todo list. "
                          "Actions: add, remove, complete, list, clear.");
}

QJsonObject TodoWriteTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("action")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("enum")] =
            QJsonArray{QStringLiteral("add"), QStringLiteral("remove"),
                       QStringLiteral("complete"), QStringLiteral("list"),
                       QStringLiteral("clear")};
        obj[QStringLiteral("description")] =
            QStringLiteral("The action to perform on the todo list");
        return obj;
    }();

    props[QStringLiteral("subject")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("The subject of the todo item "
                           "(required for add/remove/complete)");
        return obj;
    }();

    props[QStringLiteral("description")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Optional description for the todo item (for add)");
        return obj;
    }();

    props[QStringLiteral("status")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("enum")] =
            QJsonArray{QStringLiteral("pending"), QStringLiteral("in_progress"),
                       QStringLiteral("completed")};
        obj[QStringLiteral("description")] =
            QStringLiteral("Optional status for updating a todo item");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("action")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult TodoWriteTool::execute(const QJsonObject &params)
{
    auto action = params.value(QStringLiteral("action")).toString();

    if (action == QLatin1String("add"))
        return handleAdd(params);
    if (action == QLatin1String("remove"))
        return handleRemove(params);
    if (action == QLatin1String("complete"))
        return handleComplete(params);
    if (action == QLatin1String("list"))
        return handleList();
    if (action == QLatin1String("clear"))
        return handleClear();

    return act::core::ToolResult::err(
        act::core::errors::INVALID_PARAMS,
        QStringLiteral("Invalid action: '%1'. "
                       "Must be one of: add, remove, complete, list, clear")
            .arg(action));
}

act::core::PermissionLevel TodoWriteTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool TodoWriteTool::isThreadSafe() const
{
    return false;
}

std::optional<TodoWriteTool::Status>
TodoWriteTool::parseStatus(const QString &str)
{
    if (str == QLatin1String("pending"))
        return Status::Pending;
    if (str == QLatin1String("in_progress"))
        return Status::InProgress;
    if (str == QLatin1String("completed"))
        return Status::Completed;
    return std::nullopt;
}

QString TodoWriteTool::statusToString(Status status)
{
    switch (status)
    {
    case Status::Pending:
        return QStringLiteral("pending");
    case Status::InProgress:
        return QStringLiteral("in_progress");
    case Status::Completed:
        return QStringLiteral("completed");
    }
    return QStringLiteral("unknown");
}

act::core::ToolResult TodoWriteTool::handleAdd(const QJsonObject &params)
{
    auto subject = params.value(QStringLiteral("subject")).toString();
    if (subject.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("add action requires 'subject' parameter"));
    }

    TodoItem item;
    item.subject = subject;
    item.description =
        params.value(QStringLiteral("description")).toString();
    item.createdAt = QDateTime::currentDateTime();

    auto statusStr =
        params.value(QStringLiteral("status")).toString();
    if (!statusStr.isEmpty())
    {
        auto parsed = parseStatus(statusStr);
        if (parsed)
            item.status = *parsed;
    }

    m_items.append(item);

    spdlog::info("TodoWriteTool: added item '{}'", subject.toStdString());

    QJsonObject meta;
    meta[QStringLiteral("index")] = m_items.size() - 1;
    meta[QStringLiteral("total")] = m_items.size();

    return act::core::ToolResult::ok(
        QStringLiteral("Added todo: %1 (item #%2)").arg(subject).arg(m_items.size()),
        meta);
}

act::core::ToolResult TodoWriteTool::handleRemove(const QJsonObject &params)
{
    auto subject = params.value(QStringLiteral("subject")).toString();
    if (subject.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("remove action requires 'subject' parameter"));
    }

    for (int i = 0; i < m_items.size(); ++i)
    {
        if (m_items[i].subject == subject)
        {
            m_items.removeAt(i);

            spdlog::info("TodoWriteTool: removed item '{}'",
                         subject.toStdString());

            return act::core::ToolResult::ok(
                QStringLiteral("Removed todo: %1").arg(subject),
                {{QStringLiteral("total"), m_items.size()}});
        }
    }

    return act::core::ToolResult::err(
        act::core::errors::STRING_NOT_FOUND,
        QStringLiteral("Todo item not found: %1").arg(subject));
}

act::core::ToolResult TodoWriteTool::handleComplete(const QJsonObject &params)
{
    auto subject = params.value(QStringLiteral("subject")).toString();
    if (subject.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("complete action requires 'subject' parameter"));
    }

    for (auto &item : m_items)
    {
        if (item.subject == subject)
        {
            item.status = Status::Completed;

            spdlog::info("TodoWriteTool: completed item '{}'",
                         subject.toStdString());

            return act::core::ToolResult::ok(
                QStringLiteral("Completed todo: %1").arg(subject));
        }
    }

    return act::core::ToolResult::err(
        act::core::errors::STRING_NOT_FOUND,
        QStringLiteral("Todo item not found: %1").arg(subject));
}

act::core::ToolResult TodoWriteTool::handleList()
{
    if (m_items.isEmpty())
    {
        return act::core::ToolResult::ok(
            QStringLiteral("Todo list is empty."),
            {{QStringLiteral("count"), 0}});
    }

    QStringList lines;
    lines.reserve(m_items.size() + 1);
    lines.append(QStringLiteral("Todo List (%1 items):").arg(m_items.size()));

    for (int i = 0; i < m_items.size(); ++i)
    {
        const auto &item = m_items[i];
        QString marker;
        switch (item.status)
        {
        case Status::Pending:
            marker = QStringLiteral("[ ]");
            break;
        case Status::InProgress:
            marker = QStringLiteral("[~]");
            break;
        case Status::Completed:
            marker = QStringLiteral("[x]");
            break;
        }

        QString line = QStringLiteral("%1 #%2 %3")
                           .arg(marker)
                           .arg(i + 1)
                           .arg(item.subject);

        if (!item.description.isEmpty())
            line += QStringLiteral(" -- %1").arg(item.description);

        lines.append(line);
    }

    QString output = lines.join(QLatin1Char('\n'));

    QJsonObject meta;
    meta[QStringLiteral("count")] = m_items.size();
    meta[QStringLiteral("completed")] =
        std::ranges::count_if(m_items,
                              [](const TodoItem &it)
                              { return it.status == Status::Completed; });
    meta[QStringLiteral("pending")] =
        std::ranges::count_if(m_items,
                              [](const TodoItem &it)
                              { return it.status == Status::Pending; });

    return act::core::ToolResult::ok(output, meta);
}

act::core::ToolResult TodoWriteTool::handleClear()
{
    int removed = m_items.size();
    m_items.clear();

    spdlog::info("TodoWriteTool: cleared {} items", removed);

    return act::core::ToolResult::ok(
        QStringLiteral("Cleared %1 todo items.").arg(removed),
        {{QStringLiteral("removed"), removed}});
}

} // namespace act::harness
