#include "harness/tools/directory_tool.h"

#include <QDir>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

DirectoryTool::DirectoryTool(act::infrastructure::IFileSystem &fs,
                             QString workspaceRoot)
    : m_fs(fs)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString DirectoryTool::name() const
{
    return QStringLiteral("directory_create");
}

QString DirectoryTool::description() const
{
    return QStringLiteral("Create a directory (and any missing parents) "
                          "within the workspace. Like mkdir -p. "
                          "Requires write permission.");
}

QJsonObject DirectoryTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("path")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Path to the directory to create (relative or "
                          "absolute, must be within workspace)");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("path")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult DirectoryTool::execute(const QJsonObject &params)
{
    auto pathValue = params.value(QStringLiteral("path"));
    if (!pathValue.isString() || pathValue.toString().isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'path' parameter must be a non-empty string"));
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

    if (m_fs.exists(normalizedPath))
    {
        QJsonObject metadata;
        metadata[QStringLiteral("path")] = normalizedPath;
        metadata[QStringLiteral("already_existed")] = true;
        return act::core::ToolResult::ok(
            QStringLiteral("Directory already exists: %1").arg(normalizedPath),
            metadata);
    }

    spdlog::info("DirectoryTool: creating {}", normalizedPath.toStdString());

    if (!m_fs.createDirectory(normalizedPath))
    {
        return act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Failed to create directory: %1")
                .arg(normalizedPath));
    }

    QJsonObject metadata;
    metadata[QStringLiteral("path")] = normalizedPath;
    metadata[QStringLiteral("already_existed")] = false;
    return act::core::ToolResult::ok(
        QStringLiteral("Created directory: %1").arg(normalizedPath), metadata);
}

act::core::PermissionLevel DirectoryTool::permissionLevel() const
{
    return act::core::PermissionLevel::Write;
}

bool DirectoryTool::isThreadSafe() const
{
    return false;
}

bool DirectoryTool::isPathWithinWorkspace(const QString &normalizedPath) const
{
    if (!normalizedPath.startsWith(m_workspaceRoot))
        return false;
    if (normalizedPath == m_workspaceRoot)
        return true;
    const QString remainder = normalizedPath.mid(m_workspaceRoot.length());
    if (!remainder.startsWith(QLatin1Char('/')) &&
        !remainder.startsWith(QLatin1Char('\\')))
        return false;
    return true;
}

} // namespace act::harness
