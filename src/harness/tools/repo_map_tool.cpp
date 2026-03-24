#include "harness/tools/repo_map_tool.h"

#include <QDir>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

RepoMapTool::RepoMapTool(act::infrastructure::IFileSystem &fs,
                         act::infrastructure::IProcess &proc,
                         QString workspaceRoot)
    : m_fs(fs)
    , m_proc(proc)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString RepoMapTool::name() const
{
    return QStringLiteral("repo_map");
}

QString RepoMapTool::description() const
{
    return QStringLiteral("Build a file-tree map of the workspace. "
                          "Returns a tree-like representation showing "
                          "directory structure and key files.");
}

QJsonObject RepoMapTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("max_depth")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("integer");
        obj[QStringLiteral("description")] =
            QStringLiteral("Maximum directory depth (default: 3)");
        return obj;
    }();

    props[QStringLiteral("path")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Relative path within workspace to map "
                           "(default: workspace root)");
        return obj;
    }();

    props[QStringLiteral("pattern")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("File glob pattern to filter "
                           "(e.g. \"*.cpp\", \"*.h\")");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult RepoMapTool::execute(const QJsonObject &params)
{
    int maxDepth = params.value(QStringLiteral("max_depth")).toInt(
        DEFAULT_MAX_DEPTH);
    if (maxDepth < 1)
        maxDepth = 1;
    if (maxDepth > 10)
        maxDepth = 10;

    QString subPath = params.value(QStringLiteral("path")).toString();
    QString targetDir = m_workspaceRoot;
    if (!subPath.isEmpty())
    {
        targetDir = QDir(m_workspaceRoot).filePath(subPath);
    }

    auto normalized = m_fs.normalizePath(targetDir);
    if (!m_fs.exists(normalized))
    {
        return act::core::ToolResult::err(
            act::core::errors::FILE_NOT_FOUND,
            QStringLiteral("Directory not found: %1").arg(targetDir));
    }

    // Count entries
    int fileCount = 0;
    int dirCount = 0;
    countEntries(normalized, fileCount, dirCount);

    // Build tree
    QString tree = buildTree(normalized, QString(), maxDepth, 0);

    // Build git branch info if in a git repo
    QString branchInfo;
    int branchExitCode = -1;
    m_proc.execute(
        QStringLiteral("git"),
        QStringList{QStringLiteral("branch"), QStringLiteral("--show-current")},
        [&branchExitCode, &branchInfo](int code, QString out) {
            branchExitCode = code;
            branchInfo = std::move(out);
        });

    QString header = QStringLiteral("Project: %1\n")
                         .arg(QDir(m_workspaceRoot).dirName());
    if (branchExitCode == 0 && !branchInfo.trimmed().isEmpty())
    {
        header += QStringLiteral("Branch: %1\n").arg(branchInfo.trimmed());
    }
    header += QStringLiteral("Files: %1 | Directories: %2\n\n")
                  .arg(fileCount)
                  .arg(dirCount);

    return act::core::ToolResult::ok(header + tree);
}

QString RepoMapTool::buildTree(const QString &dirPath,
                               const QString &prefix,
                               int maxDepth,
                               int currentDepth) const
{
    if (currentDepth >= maxDepth)
        return {};

    auto files = m_fs.listFiles(dirPath);
    if (files.isEmpty())
        return {};

    // Separate directories and files using IFileSystem
    QStringList dirs;
    QStringList regularFiles;
    for (const auto &entry : files)
    {
        QString fullPath = QDir(dirPath).filePath(entry);
        if (entry == QStringLiteral(".") || entry == QStringLiteral(".."))
            continue;

        // A directory has listable entries; a file does not
        if (!m_fs.listFiles(fullPath).isEmpty())
            dirs.append(entry);
        else
            regularFiles.append(entry);
    }

    dirs.sort(Qt::CaseInsensitive);
    regularFiles.sort(Qt::CaseInsensitive);

    QString result;
    QStringList allEntries;
    allEntries << dirs << regularFiles;

    for (int i = 0; i < allEntries.size(); ++i)
    {
        const auto &entry = allEntries[i];
        bool isLast = (i == allEntries.size() - 1);
        QString connector = isLast ? QStringLiteral("`-- ")
                                   : QStringLiteral("|-- ");
        QString childPrefix = isLast ? QStringLiteral("    ")
                                     : QStringLiteral("|   ");

        bool isDir = dirs.contains(entry);
        QString displayName = isDir
                                 ? entry + QStringLiteral("/")
                                 : entry;

        result += prefix + connector + displayName + QStringLiteral("\n");

        if (isDir)
        {
            QString subDir = QDir(dirPath).filePath(entry);
            result += buildTree(subDir, prefix + childPrefix,
                                maxDepth, currentDepth + 1);
        }
    }

    return result;
}

void RepoMapTool::countEntries(const QString &dirPath,
                               int &fileCount,
                               int &dirCount) const
{
    auto files = m_fs.listFiles(dirPath);
    for (const auto &entry : files)
    {
        if (entry == QStringLiteral(".") || entry == QStringLiteral(".."))
            continue;

        QString fullPath = QDir(dirPath).filePath(entry);
        QFileInfo fi(fullPath);
        if (fi.isDir())
        {
            ++dirCount;
            countEntries(fullPath, fileCount, dirCount);
        }
        else
        {
            ++fileCount;
        }
    }
}

act::core::PermissionLevel RepoMapTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool RepoMapTool::isThreadSafe() const
{
    return true;
}

} // namespace act::harness
