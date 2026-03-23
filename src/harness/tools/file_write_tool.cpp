#include "harness/tools/file_write_tool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

FileWriteTool::FileWriteTool(act::infrastructure::IFileSystem &fs,
                           QString workspaceRoot)
    : m_fs(fs)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString FileWriteTool::name() const
{
    return QStringLiteral("file_write");
}

QString FileWriteTool::description() const
{
    return QStringLiteral("Write content to a file. Creates the file if it doesn't exist, "
                          "or overwrites if it does. Requires write permission.");
}

QJsonObject FileWriteTool::schema() const
{
    QJsonObject props;
    props[QStringLiteral("path")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Path to the file to write");
        return obj;
    }();

    props[QStringLiteral("content")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Content to write to the file");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("path"), QStringLiteral("content")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult FileWriteTool::execute(const QJsonObject &params)
{
    auto pathValue = params.value(QStringLiteral("path"));
    if (!pathValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'path' parameter must be a string"));
    }

    auto contentValue = params.value(QStringLiteral("content"));
    if (!contentValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'content' parameter must be a string"));
    }

    const QString rawPath = pathValue.toString();
    const QString content = contentValue.toString();
    const QString normalizedPath = m_fs.normalizePath(rawPath);

    if (!isPathWithinWorkspace(normalizedPath))
    {
        return act::core::ToolResult::err(
            act::core::errors::OUTSIDE_WORKSPACE,
            QStringLiteral("Path '%1' is outside the workspace")
                .arg(normalizedPath));
    }

    if (!m_fs.writeFile(normalizedPath, content))
    {
        return act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Failed to write file: %1").arg(normalizedPath));
    }

    QJsonObject metadata;
    metadata[QStringLiteral("path")] = normalizedPath;
    metadata[QStringLiteral("bytesWritten")] = content.size();

    return act::core::ToolResult::ok(
        QStringLiteral("Wrote %1 bytes to %2")
            .arg(content.size())
            .arg(normalizedPath),
        metadata);
}

act::core::PermissionLevel FileWriteTool::permissionLevel() const
{
    return act::core::PermissionLevel::Write;
}

bool FileWriteTool::isThreadSafe() const
{
    return false;
}

bool FileWriteTool::isPathWithinWorkspace(const QString &normalizedPath) const
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
