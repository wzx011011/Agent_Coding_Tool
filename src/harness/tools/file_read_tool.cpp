#include "harness/tools/file_read_tool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

FileReadTool::FileReadTool(act::infrastructure::IFileSystem &fs,
                           QString workspaceRoot)
    : m_fs(fs)
    , m_workspaceRoot(std::move(workspaceRoot))
{
}

QString FileReadTool::name() const
{
    return QStringLiteral("file_read");
}

QString FileReadTool::description() const
{
    return QStringLiteral("Read the contents of a file. Returns the file content as a string. "
                          "Rejects binary files and paths outside the workspace.");
}

QJsonObject FileReadTool::schema() const
{
    QJsonObject props;
    props[QStringLiteral("path")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] = QStringLiteral("Absolute or relative path to the file");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{QStringLiteral("path")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult FileReadTool::execute(const QJsonObject &params)
{
    const auto pathValue = params.value(QStringLiteral("path"));
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
        spdlog::warn("FileReadTool: path outside workspace: {}",
                     normalizedPath.toStdString());
        return act::core::ToolResult::err(
            act::core::errors::OUTSIDE_WORKSPACE,
            QStringLiteral("Path '%1' is outside the workspace").arg(normalizedPath));
    }

    if (!m_fs.exists(normalizedPath))
    {
        spdlog::warn("FileReadTool: file not found: {}",
                     normalizedPath.toStdString());
        return act::core::ToolResult::err(
            act::core::errors::FILE_NOT_FOUND,
            QStringLiteral("File not found: %1").arg(normalizedPath));
    }

    QString content;
    if (!m_fs.readFile(normalizedPath, content))
    {
        spdlog::warn("FileReadTool: failed to read file: {}",
                     normalizedPath.toStdString());
        return act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Failed to read file: %1").arg(normalizedPath));
    }

    // Binary detection: read raw bytes and check for null bytes
    QFile file(normalizedPath);
    if (file.open(QIODevice::ReadOnly))
    {
        constexpr qsizetype kMaxCheckBytes = 8192;
        const QByteArray head = file.read(kMaxCheckBytes);
        file.close();

        if (isBinaryContent(head))
        {
            spdlog::warn("FileReadTool: binary file detected: {}",
                         normalizedPath.toStdString());
            return act::core::ToolResult::err(
                act::core::errors::BINARY_FILE,
                QStringLiteral("File appears to be binary: %1").arg(normalizedPath));
        }
    }

    QJsonObject metadata;
    metadata[QStringLiteral("path")] = normalizedPath;

    return act::core::ToolResult::ok(content, metadata);
}

act::core::PermissionLevel FileReadTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool FileReadTool::isThreadSafe() const
{
    return true;
}

bool FileReadTool::isPathWithinWorkspace(const QString &normalizedPath) const
{
    const QString normWorkspace = QDir::cleanPath(m_workspaceRoot);

    // The path must start with the workspace root (after both are normalized)
    // Ensure the match is at a directory boundary
    if (!normalizedPath.startsWith(normWorkspace))
    {
        return false;
    }

    // If path equals workspace root exactly, that's a directory, not a file
    if (normalizedPath == normWorkspace)
    {
        return false;
    }

    // Ensure the character after the workspace prefix is a separator
    const QString remainder = normalizedPath.mid(normWorkspace.length());
    if (!remainder.startsWith(QLatin1Char('/')) && !remainder.startsWith(QLatin1Char('\\')))
    {
        return false;
    }

    return true;
}

bool FileReadTool::isBinaryContent(const QByteArray &data)
{
    // Heuristic: if any of the first 8192 bytes is a null byte, treat as binary
    const qsizetype checkLen = qMin(data.size(), 8192);
    for (qsizetype i = 0; i < checkLen; ++i)
    {
        if (static_cast<unsigned char>(data[i]) == 0x00)
        {
            return true;
        }
    }
    return false;
}

} // namespace act::harness
