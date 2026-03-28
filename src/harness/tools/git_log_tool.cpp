#include "harness/tools/git_log_tool.h"

#include <QDir>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

GitLogTool::GitLogTool(act::infrastructure::IProcess &proc,
                       QString workspaceRoot)
    : m_proc(proc)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString GitLogTool::name() const
{
    return QStringLiteral("git_log");
}

QString GitLogTool::description() const
{
    return QStringLiteral("Show commit history using git log --oneline "
                          "with optional filtering by count, author, "
                          "grep, and date.");
}

QJsonObject GitLogTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("count")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("integer");
        obj[QStringLiteral("description")] =
            QStringLiteral("Maximum number of commits to show");
        return obj;
    }();

    props[QStringLiteral("author")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Filter commits by author name or email");
        return obj;
    }();

    props[QStringLiteral("grep")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Filter commits by message pattern");
        return obj;
    }();

    props[QStringLiteral("since")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Show commits since this date (e.g. '2024-01-01')");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult GitLogTool::execute(const QJsonObject &params)
{
    QStringList args;
    args << QStringLiteral("log") << QStringLiteral("--oneline");

    auto countValue = params.value(QStringLiteral("count"));
    if (countValue.isDouble())
    {
        int count = countValue.toInt();
        if (count > 0)
            args << QStringLiteral("-n%1").arg(count);
    }

    auto authorValue = params.value(QStringLiteral("author"));
    if (authorValue.isString())
    {
        QString author = authorValue.toString();
        if (!author.isEmpty())
            args << QStringLiteral("--author=%1").arg(author);
    }

    auto grepValue = params.value(QStringLiteral("grep"));
    if (grepValue.isString())
    {
        QString pattern = grepValue.toString();
        if (!pattern.isEmpty())
            args << QStringLiteral("--grep=%1").arg(pattern);
    }

    auto sinceValue = params.value(QStringLiteral("since"));
    if (sinceValue.isString())
    {
        QString since = sinceValue.toString();
        if (!since.isEmpty())
            args << QStringLiteral("--since=%1").arg(since);
    }

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
            act::core::errors::NOT_GIT_REPO,
            QStringLiteral("Not a git repository: %1").arg(m_workspaceRoot));
    }

    return act::core::ToolResult::ok(output);
}

act::core::PermissionLevel GitLogTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool GitLogTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
