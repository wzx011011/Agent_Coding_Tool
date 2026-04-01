#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

/// Tool that removes or keeps a git worktree created by enter_worktree.
class ExitWorktreeTool : public ITool
{
public:
    explicit ExitWorktreeTool(act::infrastructure::IProcess &proc,
                              QString workspaceRoot);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    act::infrastructure::IProcess &m_proc;
    QString m_workspaceRoot;
};

} // namespace act::harness
