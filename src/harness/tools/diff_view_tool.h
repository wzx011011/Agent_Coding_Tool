#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

/// Read-only tool that displays file diffs with structured output.
/// Shows changes in a unified diff format with file context and
/// change statistics, suitable for PatchTransaction preview.
class DiffViewTool : public ITool
{
public:
    explicit DiffViewTool(act::infrastructure::IProcess &proc,
                          act::infrastructure::IFileSystem &fs,
                          QString workspaceRoot);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    /// Parse diff output into a structured summary.
    [[nodiscard]] QString formatDiffSummary(
        const QString &rawDiff) const;

    act::infrastructure::IProcess &m_proc;
    act::infrastructure::IFileSystem &m_fs;
    QString m_workspaceRoot;
};

} // namespace act::harness
