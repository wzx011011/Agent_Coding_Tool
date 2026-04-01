#include "harness/tools/exit_worktree_tool.h"

#include <QDir>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

ExitWorktreeTool::ExitWorktreeTool(act::infrastructure::IProcess &proc,
                                   QString workspaceRoot)
    : m_proc(proc)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString ExitWorktreeTool::name() const
{
    return QStringLiteral("exit_worktree");
}

QString ExitWorktreeTool::description() const
{
    return QStringLiteral(
        "Remove or keep an isolated git worktree. Use action='remove' to delete "
        "the worktree and its branch, or action='keep' to leave it on disk.");
}

QJsonObject ExitWorktreeTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("action")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("enum")] = QJsonArray{
            QStringLiteral("keep"),
            QStringLiteral("remove"),
        };
        obj[QStringLiteral("description")] =
            QStringLiteral("Action: 'remove' deletes the worktree, 'keep' leaves it on disk");
        return obj;
    }();

    props[QStringLiteral("worktree_path")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Path to the worktree directory to exit from");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("action")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult ExitWorktreeTool::execute(const QJsonObject &params)
{
    auto actionValue = params.value(QStringLiteral("action"));
    if (!actionValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'action' parameter must be 'keep' or 'remove'"));
    }

    const QString action = actionValue.toString();

    if (action != QStringLiteral("keep") && action != QStringLiteral("remove"))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Invalid action '%1'. Must be 'keep' or 'remove'").arg(action));
    }

    // Resolve worktree path
    QString worktreePath = params.value(QStringLiteral("worktree_path")).toString();
    if (worktreePath.isEmpty())
    {
        // Default: use workspace root (assuming we are inside a worktree)
        worktreePath = m_workspaceRoot;
    }

    // Validate path is within workspace tree
    const QString normalized = QDir::cleanPath(worktreePath);
    if (!normalized.startsWith(m_workspaceRoot))
    {
        return act::core::ToolResult::err(
            act::core::errors::OUTSIDE_WORKSPACE,
            QStringLiteral("Worktree path '%1' is outside workspace").arg(normalized));
    }

    if (action == QStringLiteral("keep"))
    {
        spdlog::info("Keeping worktree at path: {}", normalized.toStdString());

        QJsonObject metadata;
        metadata[QStringLiteral("worktree_path")] = normalized;
        metadata[QStringLiteral("action")] = QStringLiteral("keep");

        return act::core::ToolResult::ok(
            QStringLiteral("Worktree kept at: %1").arg(normalized),
            metadata);
    }

    // action == "remove": run git worktree remove
    QStringList args;
    args << QStringLiteral("worktree")
         << QStringLiteral("remove")
         << normalized;

    QString output;
    int exitCode = -1;

    m_proc.execute(QStringLiteral("git"), args,
        [&output, &exitCode](int code, QString out) {
            exitCode = code;
            output = std::move(out);
        });

    if (exitCode != 0)
    {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("Failed to remove worktree '%1': %2")
                .arg(normalized, output.trimmed()));
    }

    spdlog::info("Removed worktree at path: {}", normalized.toStdString());

    QJsonObject metadata;
    metadata[QStringLiteral("worktree_path")] = normalized;
    metadata[QStringLiteral("action")] = QStringLiteral("remove");

    return act::core::ToolResult::ok(
        QStringLiteral("Removed worktree at: %1").arg(normalized),
        metadata);
}

act::core::PermissionLevel ExitWorktreeTool::permissionLevel() const
{
    return act::core::PermissionLevel::Destructive;
}

bool ExitWorktreeTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
