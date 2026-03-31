#pragma once

#include <QString>

#include "harness/interfaces.h"

namespace act::framework
{
class AgentLoop;
}

namespace act::harness
{

/// Read-only tool that exits Plan Mode on the AgentLoop.
class ExitPlanModeTool : public ITool
{
public:
    explicit ExitPlanModeTool(act::framework::AgentLoop &loop);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    act::framework::AgentLoop &m_loop;
};

} // namespace act::harness
