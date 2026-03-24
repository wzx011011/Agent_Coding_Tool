#include "harness/tools/grep_tool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTextStream>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

GrepTool::GrepTool(act::infrastructure::IFileSystem &fs,
                   QString workspaceRoot)
    : m_fs(fs)
    , m_workspaceRoot(std::move(workspaceRoot))
{
}

QString GrepTool::name() const
{
    return QStringLiteral("grep");
}

QString GrepTool::description() const
{
    return QStringLiteral("Search file contents using a regular expression pattern. "
                          "Returns matching lines with line numbers. "
                          "Can search a single file or all files in a directory.");
}

QJsonObject GrepTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("pattern")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] = QStringLiteral("Regular expression pattern to search for");
        return obj;
    }();

    props[QStringLiteral("path")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] = QStringLiteral("File or directory path to search in. "
                                                            "Defaults to workspace root.");
        return obj;
    }();

    props[QStringLiteral("glob")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] = QStringLiteral("File glob filter (e.g. '*.cpp'). "
                                                            "Only used when path is a directory.");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{QStringLiteral("pattern")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult GrepTool::execute(const QJsonObject &params)
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
    const QRegularExpression regex(pattern);
    if (!regex.isValid())
    {
        spdlog::warn("GrepTool: invalid regex pattern: {}",
                     pattern.toStdString());
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PATTERN,
            QStringLiteral("Invalid regular expression: %1").arg(regex.errorString()));
    }

    // Determine search path
    QString searchPath = m_workspaceRoot;
    const auto pathValue = params.value(QStringLiteral("path"));
    if (pathValue.isString() && !pathValue.toString().isEmpty())
    {
        searchPath = m_fs.normalizePath(pathValue.toString());
    }

    if (!isPathWithinWorkspace(searchPath))
    {
        spdlog::warn("GrepTool: path outside workspace: {}",
                     searchPath.toStdString());
        return act::core::ToolResult::err(
            act::core::errors::OUTSIDE_WORKSPACE,
            QStringLiteral("Path '%1' is outside the workspace").arg(searchPath));
    }

    const QFileInfo pathInfo(searchPath);

    if (!pathInfo.exists())
    {
        return act::core::ToolResult::err(
            act::core::errors::FILE_NOT_FOUND,
            QStringLiteral("Path not found: %1").arg(searchPath));
    }

    if (pathInfo.isFile())
    {
        // Search single file
        const QString result = grepFile(searchPath, regex);
        if (result.isEmpty())
        {
            return act::core::ToolResult::ok(
                QStringLiteral("(no matches found)"),
                {{QStringLiteral("matchCount"), 0}});
        }

        return act::core::ToolResult::ok(result);
    }

    if (pathInfo.isDir())
    {
        // Search all files in directory
        const QString globFilter = params.value(QStringLiteral("glob")).toString();
        const QString patternToUse = globFilter.isEmpty()
            ? QStringLiteral("*")
            : globFilter;

        const QStringList files = m_fs.listFiles(searchPath, patternToUse);

        QString allResults;
        int totalMatches = 0;

        for (const QString &file : files)
        {
            const QString fullPath = QDir::cleanPath(searchPath + QLatin1Char('/') + file);
            const QFileInfo fi(fullPath);

            // Skip directories and binary-looking files
            if (!fi.exists() || fi.isDir())
            {
                continue;
            }

            const QString fileResult = grepFile(fullPath, regex);
            if (!fileResult.isEmpty())
            {
                if (!allResults.isEmpty())
                {
                    allResults += QLatin1Char('\n');
                }
                allResults += fileResult;
                totalMatches++;
            }
        }

        if (allResults.isEmpty())
        {
            return act::core::ToolResult::ok(
                QStringLiteral("(no matches found)"),
                {{QStringLiteral("matchCount"), 0}});
        }

        return act::core::ToolResult::ok(allResults,
            {{QStringLiteral("matchCount"), totalMatches}});
    }

    return act::core::ToolResult::err(
        act::core::errors::INVALID_PARAMS,
        QStringLiteral("Path '%1' is neither a file nor a directory").arg(searchPath));
}

act::core::PermissionLevel GrepTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool GrepTool::isThreadSafe() const
{
    return true;
}

QString GrepTool::grepFile(const QString &filePath,
                           const QRegularExpression &regex) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString results;
    int lineNumber = 0;

    while (!stream.atEnd())
    {
        lineNumber++;
        const QString line = stream.readLine();

        if (regex.match(line).hasMatch())
        {
            if (!results.isEmpty())
            {
                results += QLatin1Char('\n');
            }
            results += QStringLiteral("%1:%2: %3")
                .arg(filePath)
                .arg(lineNumber)
                .arg(line);
        }
    }

    file.close();
    return results;
}

bool GrepTool::isPathWithinWorkspace(const QString &normalizedPath) const
{
    const QString normWorkspace = QDir::cleanPath(m_workspaceRoot);

    if (!normalizedPath.startsWith(normWorkspace))
    {
        return false;
    }

    // Path equals workspace root means search the whole workspace - allowed
    if (normalizedPath == normWorkspace)
    {
        return true;
    }

    // Ensure the character after the workspace prefix is a separator
    const QString remainder = normalizedPath.mid(normWorkspace.length());
    if (!remainder.startsWith(QLatin1Char('/')) && !remainder.startsWith(QLatin1Char('\\')))
    {
        return false;
    }

    return true;
}

} // namespace act::harness
