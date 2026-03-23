#include "harness/tools/glob_tool.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

GlobTool::GlobTool(act::infrastructure::IFileSystem &fs,
                   QString workspaceRoot)
    : m_fs(fs)
    , m_workspaceRoot(std::move(workspaceRoot))
{
}

QString GlobTool::name() const
{
    return QStringLiteral("glob");
}

QString GlobTool::description() const
{
    return QStringLiteral("Find files matching a glob pattern in the workspace. "
                          "Returns a list of matching file paths.");
}

QJsonObject GlobTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("pattern")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] = QStringLiteral("Glob pattern to match files "
                                                            "(e.g. '*.cpp', '**/*.h')");
        return obj;
    }();

    props[QStringLiteral("path")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] = QStringLiteral("Directory to search in. "
                                                            "Defaults to workspace root.");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{QStringLiteral("pattern")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult GlobTool::execute(const QJsonObject &params)
{
    // Validate pattern parameter
    const auto patternValue = params.value(QStringLiteral("pattern"));
    if (!patternValue.isString() || patternValue.toString().isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'pattern' parameter must be a non-empty string"));
    }

    const QString pattern = patternValue.toString();

    // Determine search directory
    QString searchDir = m_workspaceRoot;
    const auto pathValue = params.value(QStringLiteral("path"));
    if (pathValue.isString() && !pathValue.toString().isEmpty())
    {
        searchDir = m_fs.normalizePath(pathValue.toString());
    }

    if (!isPathWithinWorkspace(searchDir))
    {
        spdlog::warn("GlobTool: path outside workspace: {}",
                     searchDir.toStdString());
        return act::core::ToolResult::err(
            act::core::errors::OUTSIDE_WORKSPACE,
            QStringLiteral("Path '%1' is outside the workspace").arg(searchDir));
    }

    const QFileInfo dirInfo(searchDir);
    if (!dirInfo.exists() || !dirInfo.isDir())
    {
        return act::core::ToolResult::err(
            act::core::errors::FILE_NOT_FOUND,
            QStringLiteral("Directory not found: %1").arg(searchDir));
    }

    // Use QDirIterator for recursive glob support
    QStringList matches;
    QDirIterator it(searchDir, {pattern},
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        matches.append(it.next());
    }

    // Sort for deterministic output
    matches.sort(Qt::CaseInsensitive);

    QJsonObject metadata;
    metadata[QStringLiteral("matchCount")] = matches.size();

    if (matches.isEmpty())
    {
        return act::core::ToolResult::ok(
            QStringLiteral("(no files matched)"),
            metadata);
    }

    const QString output = matches.join(QLatin1Char('\n'));
    return act::core::ToolResult::ok(output, metadata);
}

act::core::PermissionLevel GlobTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool GlobTool::isThreadSafe() const
{
    return true;
}

bool GlobTool::isPathWithinWorkspace(const QString &normalizedPath) const
{
    const QString normWorkspace = QDir::cleanPath(m_workspaceRoot);

    if (!normalizedPath.startsWith(normWorkspace))
    {
        return false;
    }

    if (normalizedPath == normWorkspace)
    {
        return true;
    }

    const QString remainder = normalizedPath.mid(normWorkspace.length());
    if (!remainder.startsWith(QLatin1Char('/')) && !remainder.startsWith(QLatin1Char('\\')))
    {
        return false;
    }

    return true;
}

} // namespace act::harness
