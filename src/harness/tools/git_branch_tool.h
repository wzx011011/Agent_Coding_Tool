#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

/// Exec-level tool that manages git branches (list, create, delete, switch).
class GitBranchTool : public ITool
{
public:
    explicit GitBranchTool(act::infrastructure::IProcess &proc,
                           QString workspaceRoot);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    [[nodiscard]] act::core::ToolResult listBranches(bool showCurrent);
    [[nodiscard]] act::core::ToolResult createBranch(const QString &name,
                                                     bool switchTo);
    [[nodiscard]] act::core::ToolResult deleteBranch(const QString &name,
                                                     bool force);
    [[nodiscard]] act::core::ToolResult switchBranch(const QString &name);

    act::infrastructure::IProcess &m_proc;
    QString m_workspaceRoot;
};

} // namespace act::harness
