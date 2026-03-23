#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

class GlobTool : public ITool
{
public:
    explicit GlobTool(act::infrastructure::IFileSystem &fs,
                      QString workspaceRoot);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    act::core::ToolResult execute(const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    // Check that a path is within the workspace root
    [[nodiscard]] bool isPathWithinWorkspace(const QString &normalizedPath) const;

    act::infrastructure::IFileSystem &m_fs;
    QString m_workspaceRoot;
};

} // namespace act::harness
