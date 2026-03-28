#include "harness/tools/file_delete_tool.h"

#include <QDir>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

FileDeleteTool::FileDeleteTool(act::infrastructure::IFileSystem &fs,
                               QString workspaceRoot)
    : m_fs(fs)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString FileDeleteTool::name() const
{
    return QStringLiteral("file_delete");
}

QString FileDeleteTool::description() const
{
    return QStringLiteral("Delete a file from the workspace. "
                          "The path must be within the workspace root. "
                          "Requires destructive permission.");
}

QJsonObject FileDeleteTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("path")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Path to the file to delete (relative or absolute, "
                          "must be within workspace)");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("path")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult FileDeleteTool::execute(const QJsonObject &params)
{
    auto pathValue = params.value(QStringLiteral("path"));
    if (!pathValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'path' parameter must be a string"));
    }

    const QString rawPath = pathValue.toString();
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

    spdlog::info("FileDeleteTool: deleting {}", normalizedPath.toStdString());

    if (!m_fs.removeFile(normalizedPath))
    {
        return act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Failed to delete file: %1").arg(normalizedPath));
    }

    QJsonObject metadata;
    metadata[QStringLiteral("path")] = normalizedPath;
    return act::core::ToolResult::ok(
        QStringLiteral("Deleted file: %1").arg(normalizedPath), metadata);
}

act::core::PermissionLevel FileDeleteTool::permissionLevel() const
{
    return act::core::PermissionLevel::Destructive;
}

bool FileDeleteTool::isThreadSafe() const
{
    return false;
}

bool FileDeleteTool::isPathWithinWorkspace(const QString &normalizedPath) const
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

} // namespace act::harness
