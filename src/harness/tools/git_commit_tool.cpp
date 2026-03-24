#include "harness/tools/git_commit_tool.h"

#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

GitCommitTool::GitCommitTool(act::infrastructure::IProcess &proc,
                             QString workspaceRoot)
    : m_proc(proc)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString GitCommitTool::name() const
{
    return QStringLiteral("git_commit");
}

QString GitCommitTool::description() const
{
    return QStringLiteral("Stage files and create a git commit. "
                          "Returns the commit hash on success.");
}

QJsonObject GitCommitTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("message")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Commit message (conventional commit format: "
                           "type(scope): description)");
        return obj;
    }();

    props[QStringLiteral("files")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("array");
        obj[QStringLiteral("items")] = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")}};
        obj[QStringLiteral("description")] =
            QStringLiteral("Files to stage before committing. "
                           "Empty means commit all staged changes.");
        return obj;
    }();

    props[QStringLiteral("allow_empty")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("boolean");
        obj[QStringLiteral("description")] =
            QStringLiteral("Allow empty commit (default: false)");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{
        QStringLiteral("message")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult GitCommitTool::execute(const QJsonObject &params)
{
    auto message = params.value(QStringLiteral("message")).toString();
    if (message.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("git_commit requires 'message' parameter"));
    }

    if (!isValidConventionalCommit(message))
    {
        spdlog::warn("git_commit: message does not follow conventional "
                     "commit format: {}",
                     message.toStdString());
    }

    // Stage files if specified
    auto filesValue = params.value(QStringLiteral("files"));
    if (filesValue.isArray())
    {
        const auto arr = filesValue.toArray();
        if (!arr.isEmpty())
        {
            QStringList addArgs;
            addArgs << QStringLiteral("add");
            addArgs << QStringLiteral("--");
            for (const auto &item : arr)
                addArgs << item.toString();

            int exitCode = -1;
            QString addOutput;
            m_proc.execute(QStringLiteral("git"), addArgs,
                [&exitCode, &addOutput](int code, QString out) {
                    exitCode = code;
                    addOutput = std::move(out);
                });

            if (exitCode != 0)
            {
                return act::core::ToolResult::err(
                    act::core::errors::NOT_GIT_REPO,
                    QStringLiteral("git add failed: %1").arg(addOutput));
            }
        }
    }

    // Commit
    QStringList commitArgs;
    commitArgs << QStringLiteral("commit");
    commitArgs << QStringLiteral("-m") << message;

    bool allowEmpty = params.value(QStringLiteral("allow_empty")).toBool(false);
    if (allowEmpty)
        commitArgs << QStringLiteral("--allow-empty");

    int commitExitCode = -1;
    QString commitOutput;
    m_proc.execute(QStringLiteral("git"), commitArgs,
        [&commitExitCode, &commitOutput](int code, QString out) {
            commitExitCode = code;
            commitOutput = std::move(out);
        });

    if (commitExitCode != 0)
    {
        return act::core::ToolResult::err(
            act::core::errors::TIMEOUT,
            QStringLiteral("git commit failed: %1").arg(commitOutput));
    }

    // Get the commit hash
    QString commitHash;
    int hashExitCode = -1;
    m_proc.execute(QStringLiteral("git"),
        QStringList{QStringLiteral("rev-parse"), QStringLiteral("HEAD")},
        [&hashExitCode, &commitHash](int code, QString out) {
            hashExitCode = code;
            commitHash = std::move(out);
        });

    if (hashExitCode != 0 || commitHash.isEmpty())
    {
        return act::core::ToolResult::ok(
            QStringLiteral("Commit created (hash unavailable): %1")
                .arg(commitOutput.trimmed()));
    }

    return act::core::ToolResult::ok(
        QStringLiteral("Committed %1").arg(commitHash.trimmed()));
}

act::core::PermissionLevel GitCommitTool::permissionLevel() const
{
    return act::core::PermissionLevel::Write;
}

bool GitCommitTool::isThreadSafe() const
{
    return false;
}

bool GitCommitTool::isValidConventionalCommit(const QString &message)
{
    // Conventional commit: type(scope): description
    // e.g., feat: add new feature, fix(core): resolve crash
    static const QRegularExpression re(
        QStringLiteral("^(feat|fix|refactor|docs|test|chore|perf|style|"
                       "build|ci|revert)(\\(.+\\))?!?:\\s.+"));
    return re.match(message).hasMatch();
}

} // namespace act::harness
