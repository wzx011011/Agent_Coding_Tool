#include "harness/tools/diff_view_tool.h"

#include <QDir>
#include <QJsonArray>
#include <QStringList>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

DiffViewTool::DiffViewTool(act::infrastructure::IProcess &proc,
                           act::infrastructure::IFileSystem &fs,
                           QString workspaceRoot)
    : m_proc(proc)
    , m_fs(fs)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString DiffViewTool::name() const
{
    return QStringLiteral("diff_view");
}

QString DiffViewTool::description() const
{
    return QStringLiteral("Display a structured diff view of file changes. "
                          "Shows staged, unstaged, or specific file diffs "
                          "with change statistics.");
}

QJsonObject DiffViewTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("mode")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("enum")] = QJsonArray{
            QStringLiteral("staged"),
            QStringLiteral("unstaged"),
            QStringLiteral("all")};
        obj[QStringLiteral("description")] =
            QStringLiteral("Which changes to show: 'staged', 'unstaged', "
                           "or 'all' (default: 'unstaged')");
        return obj;
    }();

    props[QStringLiteral("paths")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("array");
        obj[QStringLiteral("items")] = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")}};
        obj[QStringLiteral("description")] =
            QStringLiteral("Optional file paths to filter the diff");
        return obj;
    }();

    props[QStringLiteral("stat_only")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("boolean");
        obj[QStringLiteral("description")] =
            QStringLiteral("Show only change statistics, not full diff "
                           "(default: false)");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult DiffViewTool::execute(const QJsonObject &params)
{
    QString mode = params.value(QStringLiteral("mode")).toString(
        QStringLiteral("unstaged"));
    bool statOnly =
        params.value(QStringLiteral("stat_only")).toBool(false);

    QStringList args;

    if (mode == QStringLiteral("staged"))
    {
        args << QStringLiteral("diff") << QStringLiteral("--cached")
             << QStringLiteral("--stat");
    }
    else if (mode == QStringLiteral("all"))
    {
        args << QStringLiteral("diff") << QStringLiteral("HEAD")
             << QStringLiteral("--stat");
    }
    else
    {
        args << QStringLiteral("diff") << QStringLiteral("--stat");
    }

    // Add path filter
    auto pathsValue = params.value(QStringLiteral("paths"));
    if (pathsValue.isArray())
    {
        const auto arr = pathsValue.toArray();
        if (!arr.isEmpty())
        {
            args << QStringLiteral("--");
            for (const auto &item : arr)
                args << item.toString();
        }
    }

    // First get the stat summary
    QString statOutput;
    int statExitCode = -1;
    m_proc.execute(QStringLiteral("git"), args,
        [&statExitCode, &statOutput](int code, QString out) {
            statExitCode = code;
            statOutput = std::move(out);
        });

    if (statExitCode != 0)
    {
        // Not a git repo or no changes
        if (statOutput.contains(QStringLiteral("not a git repository")))
        {
            return act::core::ToolResult::err(
                act::core::errors::NOT_GIT_REPO,
                QStringLiteral("Not a git repository: %1")
                    .arg(m_workspaceRoot));
        }
        return act::core::ToolResult::ok(
            QStringLiteral("No changes detected."));
    }

    if (statOutput.trimmed().isEmpty())
    {
        return act::core::ToolResult::ok(
            QStringLiteral("No changes detected."));
    }

    if (statOnly)
    {
        return act::core::ToolResult::ok(
            QStringLiteral("=== Change Summary ===\n") + statOutput);
    }

    // Now get the full diff
    QStringList fullArgs;
    if (mode == QStringLiteral("staged"))
    {
        fullArgs << QStringLiteral("diff") << QStringLiteral("--cached")
                 << QStringLiteral("--unified=3");
    }
    else if (mode == QStringLiteral("all"))
    {
        fullArgs << QStringLiteral("diff") << QStringLiteral("HEAD")
                 << QStringLiteral("--unified=3");
    }
    else
    {
        fullArgs << QStringLiteral("diff") << QStringLiteral("--unified=3");
    }

    if (pathsValue.isArray())
    {
        const auto arr = pathsValue.toArray();
        if (!arr.isEmpty())
        {
            fullArgs << QStringLiteral("--");
            for (const auto &item : arr)
                fullArgs << item.toString();
        }
    }

    QString fullDiff;
    int fullExitCode = -1;
    m_proc.execute(QStringLiteral("git"), fullArgs,
        [&fullExitCode, &fullDiff](int code, QString out) {
            fullExitCode = code;
            fullDiff = std::move(out);
        });

    QString result =
        QStringLiteral("=== Change Summary ===\n") + statOutput + QStringLiteral("\n");

    if (fullExitCode == 0 && !fullDiff.trimmed().isEmpty())
    {
        result += QStringLiteral("=== Full Diff ===\n") + fullDiff;
    }

    return act::core::ToolResult::ok(result);
}

QString DiffViewTool::formatDiffSummary(const QString &rawDiff) const
{
    QStringList lines = rawDiff.split('\n');
    int filesChanged = 0;
    int insertions = 0;
    int deletions = 0;

    for (const auto &line : lines)
    {
        if (line.startsWith(QStringLiteral("diff --git")))
            ++filesChanged;
        else if (line.startsWith('+') &&
                 !line.startsWith(QStringLiteral("+++")))
            ++insertions;
        else if (line.startsWith('-') &&
                 !line.startsWith(QStringLiteral("---")))
            ++deletions;
    }

    return QStringLiteral("%1 file(s) changed, %2 insertion(s), "
                         "%3 deletion(s)")
        .arg(filesChanged)
        .arg(insertions)
        .arg(deletions);
}

act::core::PermissionLevel DiffViewTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool DiffViewTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
