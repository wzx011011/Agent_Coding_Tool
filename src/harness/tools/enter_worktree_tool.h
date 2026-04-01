#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

/// Tool that creates an isolated git worktree under .claude/worktrees/<name>.
class EnterWorktreeTool : public ITool
{
public:
    explicit EnterWorktreeTool(act::infrastructure::IProcess &proc,
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
    static QString generateRandomSuffix(int length = 6);

    act::infrastructure::IProcess &m_proc;
    QString m_workspaceRoot;
};

} // namespace act::harness
