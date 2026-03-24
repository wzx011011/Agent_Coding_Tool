#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

/// Write tool that runs `git add` and `git commit` in the workspace.
class GitCommitTool : public ITool
{
public:
    explicit GitCommitTool(act::infrastructure::IProcess &proc,
                           QString workspaceRoot);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    /// Validate conventional commit format: type(scope): description
    [[nodiscard]] static bool isValidConventionalCommit(
        const QString &message);

    act::infrastructure::IProcess &m_proc;
    QString m_workspaceRoot;
};

} // namespace act::harness
