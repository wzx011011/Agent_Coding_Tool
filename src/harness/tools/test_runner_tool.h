#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

class TestRunnerTool : public ITool
{
public:
    explicit TestRunnerTool(act::infrastructure::IProcess &proc,
                            QString workspaceRoot);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

    void setTimeoutMs(int ms);

private:
    [[nodiscard]] static QJsonObject parseTestOutput(const QString &output);

    act::infrastructure::IProcess &m_proc;
    QString m_workspaceRoot;
    int m_timeoutMs = 120000; // 2 minutes default for tests
};

} // namespace act::harness
