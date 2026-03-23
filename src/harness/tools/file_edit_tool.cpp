#include "harness/tools/file_edit_tool.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

FileEditTool::FileEditTool(act::infrastructure::IFileSystem &fs,
                          QString workspaceRoot)
    : m_fs(fs)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString FileEditTool::name() const
{
    return QStringLiteral("file_edit");
}

QString FileEditTool::description() const
{
    return QStringLiteral("Replace an exact string in a file with a new string. "
                          "The old_string must match exactly one occurrence in the file. "
                          "Requires write permission.");
}

QJsonObject FileEditTool::schema() const
{
    QJsonObject props;
    props[QStringLiteral("path")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Path to the file to edit");
        return obj;
    }();

    props[QStringLiteral("old_string")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("The exact string to replace (must match exactly "
                          "one occurrence)");
        return obj;
    }();

    props[QStringLiteral("new_string")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("The replacement string");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("path"), QStringLiteral("old_string"),
                    QStringLiteral("new_string")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult FileEditTool::execute(const QJsonObject &params)
{
    auto pathValue = params.value(QStringLiteral("path"));
    if (!pathValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'path' parameter must be a string"));
    }

    auto oldValue = params.value(QStringLiteral("old_string"));
    if (!oldValue.isString() || oldValue.toString().isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'old_string' must be a non-empty string"));
    }

    auto newValue = params.value(QStringLiteral("new_string"));
    if (!newValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'new_string' parameter must be a string"));
    }

    const QString rawPath = pathValue.toString();
    const QString oldStr = oldValue.toString();
    const QString newStr = newValue.toString();
    const QString normalizedPath = m_fs.normalizePath(rawPath);

    if (!isPathWithinWorkspace(normalizedPath))
    {
        return act::core::ToolResult::err(
            act::core::errors::OUTSIDE_WORKSPACE,
            QStringLiteral("Path '%1' is outside the workspace")
                .arg(normalizedPath));
    }

    if (!m_fs.exists(normalizedPath))
    {
        return act::core::ToolResult::err(
            act::core::errors::FILE_NOT_FOUND,
            QStringLiteral("File not found: %1").arg(normalizedPath));
    }

    QString content;
    if (!m_fs.readFile(normalizedPath, content))
    {
        return act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Failed to read file: %1").arg(normalizedPath));
    }

    auto result = applyEdit(content, oldStr, newStr);
    if (!result.success)
        return result;

    if (!m_fs.writeFile(normalizedPath, result.output))
    {
        return act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Failed to write edited file: %1")
                .arg(normalizedPath));
    }

    QJsonObject metadata;
    metadata[QStringLiteral("path")] = normalizedPath;
    return act::core::ToolResult::ok(
        QStringLiteral("Edited file: %1").arg(normalizedPath), metadata);
}

act::core::PermissionLevel FileEditTool::permissionLevel() const
{
    return act::core::PermissionLevel::Write;
}

bool FileEditTool::isThreadSafe() const
{
    return false;
}

bool FileEditTool::isPathWithinWorkspace(const QString &normalizedPath) const
{
    if (!normalizedPath.startsWith(m_workspaceRoot))
        return false;
    if (normalizedPath == m_workspaceRoot)
        return false;
    const QString remainder = normalizedPath.mid(m_workspaceRoot.length());
    if (!remainder.startsWith(QLatin1Char('/')) &&
        !remainder.startsWith(QLatin1Char('\\')))
        return false;
    return true;
}

act::core::ToolResult FileEditTool::applyEdit(
    QString &content,
    const QString &oldStr,
    const QString &newStr) const
{
    const int index = content.indexOf(oldStr);
    if (index == -1)
    {
        return act::core::ToolResult::err(
            act::core::errors::STRING_NOT_FOUND,
            QStringLiteral("old_string not found in file"));
    }

    // Check for ambiguous matches
    const int secondIndex = content.indexOf(oldStr, index + oldStr.length());
    if (secondIndex != -1)
    {
        return act::core::ToolResult::err(
            act::core::errors::AMBIGUOUS_MATCH,
            QStringLiteral("old_string matches multiple times in file"));
    }

    content.replace(index, oldStr.length(), newStr);
    return act::core::ToolResult::ok(content);
}

} // namespace act::harness
