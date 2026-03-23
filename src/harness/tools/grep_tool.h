#pragma once

#include <QRegularExpression>
#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

class GrepTool : public ITool
{
public:
    explicit GrepTool(act::infrastructure::IFileSystem &fs,
                      QString workspaceRoot);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    act::core::ToolResult execute(const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    // Grep a single file, returning matching lines
    [[nodiscard]] QString grepFile(const QString &filePath,
                                   const QRegularExpression &regex) const;

    // Check that a path is within the workspace root
    [[nodiscard]] bool isPathWithinWorkspace(const QString &normalizedPath) const;

    act::infrastructure::IFileSystem &m_fs;
    QString m_workspaceRoot;
};

} // namespace act::harness
