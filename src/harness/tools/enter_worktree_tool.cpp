#include "harness/tools/enter_worktree_tool.h"

#include <QDir>
#include <QJsonArray>
#include <QRandomGenerator>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

EnterWorktreeTool::EnterWorktreeTool(act::infrastructure::IProcess &proc,
                                     QString workspaceRoot)
    : m_proc(proc)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString EnterWorktreeTool::name() const
{
    return QStringLiteral("enter_worktree");
}

QString EnterWorktreeTool::description() const
{
    return QStringLiteral(
        "Create an isolated git worktree under .claude/worktrees/<name>. "
        "A new branch is created at HEAD. Use exit_worktree to remove or keep.");
}

QJsonObject EnterWorktreeTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("name")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Worktree name (optional, auto-generated if omitted)");
        return obj;
    }();

    props[QStringLiteral("branch")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("New branch name (optional, auto-generated if omitted)");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult EnterWorktreeTool::execute(const QJsonObject &params)
{
    // Resolve worktree name
    QString worktreeName = params.value(QStringLiteral("name")).toString();
    if (worktreeName.isEmpty())
    {
        worktreeName = QStringLiteral("act-worktree-%1").arg(generateRandomSuffix());
    }

    // Resolve branch name
    QString branchName = params.value(QStringLiteral("branch")).toString();
    if (branchName.isEmpty())
    {
        branchName = worktreeName;
    }

    // Validate names are safe (no path traversal)
    if (worktreeName.contains(QLatin1Char('/')) ||
        worktreeName.contains(QLatin1Char('\\')) ||
        worktreeName == QStringLiteral(".."))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Invalid worktree name: '%1'").arg(worktreeName));
    }

    if (branchName.contains(QLatin1Char(' ')))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Branch name must not contain spaces: '%1'").arg(branchName));
    }

    // Compute worktree path relative to workspace
    const QString worktreePath = m_workspaceRoot + QStringLiteral("/.claude/worktrees/") + worktreeName;

    // Ensure the parent directory exists
    {
        QDir dir(m_workspaceRoot);
        if (!dir.mkpath(QStringLiteral(".claude/worktrees")))
        {
            return act::core::ToolResult::err(
                act::core::errors::PERMISSION_DENIED,
                QStringLiteral("Failed to create .claude/worktrees directory"));
        }
    }

    // Run: git worktree add <path> -b <branch> HEAD
    QStringList args;
    args << QStringLiteral("worktree")
         << QStringLiteral("add")
         << worktreePath
         << QStringLiteral("-b")
         << branchName
         << QStringLiteral("HEAD");

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
            QStringLiteral("Failed to create worktree '%1': %2")
                .arg(worktreeName, output.trimmed()));
    }

    spdlog::info("Created worktree '{}' at branch '{}' => {}",
                 worktreeName.toStdString(),
                 branchName.toStdString(),
                 worktreePath.toStdString());

    QJsonObject metadata;
    metadata[QStringLiteral("worktree_path")] = worktreePath;
    metadata[QStringLiteral("branch")] = branchName;
    metadata[QStringLiteral("name")] = worktreeName;

    return act::core::ToolResult::ok(
        QStringLiteral("Created worktree '%1' at branch '%2'\nPath: %3")
            .arg(worktreeName, branchName, worktreePath),
        metadata);
}

act::core::PermissionLevel EnterWorktreeTool::permissionLevel() const
{
    return act::core::PermissionLevel::Destructive;
}

bool EnterWorktreeTool::isThreadSafe() const
{
    return false;
}

QString EnterWorktreeTool::generateRandomSuffix(int length)
{
    static const QLatin1Char kChars[] = {
        QLatin1Char('a'), QLatin1Char('b'), QLatin1Char('c'), QLatin1Char('d'),
        QLatin1Char('e'), QLatin1Char('f'), QLatin1Char('g'), QLatin1Char('h'),
        QLatin1Char('i'), QLatin1Char('j'), QLatin1Char('k'), QLatin1Char('l'),
        QLatin1Char('m'), QLatin1Char('n'), QLatin1Char('o'), QLatin1Char('p'),
        QLatin1Char('q'), QLatin1Char('r'), QLatin1Char('s'), QLatin1Char('t'),
        QLatin1Char('u'), QLatin1Char('v'), QLatin1Char('w'), QLatin1Char('x'),
        QLatin1Char('y'), QLatin1Char('z'), QLatin1Char('0'), QLatin1Char('1'),
        QLatin1Char('2'), QLatin1Char('3'), QLatin1Char('4'), QLatin1Char('5'),
        QLatin1Char('6'), QLatin1Char('7'), QLatin1Char('8'), QLatin1Char('9'),
    };
    constexpr int kCount = sizeof(kChars) / sizeof(kChars[0]);

    QString result;
    result.reserve(length);
    auto rng = QRandomGenerator::global();
    for (int i = 0; i < length; ++i)
    {
        result.append(kChars[rng->bounded(kCount)]);
    }
    return result;
}

} // namespace act::harness
