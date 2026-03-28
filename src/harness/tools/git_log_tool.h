#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

/// Read-only tool that runs git log with filtering options.
class GitLogTool : public ITool
{
public:
    explicit GitLogTool(act::infrastructure::IProcess &proc,
                        QString workspaceRoot);

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
