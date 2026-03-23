#pragma once

#include <QObject>
#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

class FileReadTool : public ITool
{
public:
    explicit FileReadTool(act::infrastructure::IFileSystem &fs,
                          QString workspaceRoot);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    act::core::ToolResult execute(const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    // Check that a path is within the workspace root (prevents directory traversal)
    [[nodiscard]] bool isPathWithinWorkspace(const QString &normalizedPath) const;

    // Detect binary content by checking for null bytes in first N bytes
    [[nodiscard]] static bool isBinaryContent(const QByteArray &data);

    act::infrastructure::IFileSystem &m_fs;
    QString m_workspaceRoot;
};

} // namespace act::harness
