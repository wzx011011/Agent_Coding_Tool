#pragma once

#include <QString>
#include <QStringList>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

class ShellExecTool : public ITool
{
public:
    explicit ShellExecTool(act::infrastructure::IProcess &proc,
                            QString workspaceRoot);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

    // Security configuration
    void setTimeoutMs(int ms);
    void addToAllowlist(const QString &command);
    void addToDenylist(const QString &command);

private:
    [[nodiscard]] bool isCommandBlocked(const QString &command) const;
    [[nodiscard]] bool isCommandAllowed(const QString &command) const;

    act::infrastructure::IProcess &m_proc;
    QString m_workspaceRoot;
    int m_timeoutMs = 30000;
    QStringList m_denylist;
    QStringList m_allowlist;
};

} // namespace act::harness
