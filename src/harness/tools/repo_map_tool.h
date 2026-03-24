#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

/// Read-only tool that builds a file-tree map of the workspace.
/// Provides the agent with a structural overview of the project.
class RepoMapTool : public ITool
{
public:
    explicit RepoMapTool(act::infrastructure::IFileSystem &fs,
                         act::infrastructure::IProcess &proc,
                         QString workspaceRoot);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    /// Recursively build a tree representation of the directory.
    QString buildTree(const QString &dirPath, const QString &prefix,
                      int maxDepth, int currentDepth) const;

    /// Count files and directories in the workspace.
    void countEntries(const QString &dirPath, int &fileCount,
                      int &dirCount) const;

    act::infrastructure::IFileSystem &m_fs;
    act::infrastructure::IProcess &m_proc;
    QString m_workspaceRoot;

    /// Maximum depth for tree traversal (default: 3)
    static constexpr int DEFAULT_MAX_DEPTH = 3;
};

} // namespace act::harness
