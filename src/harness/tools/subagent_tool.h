#pragma once

#include <QJsonObject>
#include <QString>

#include "framework/subagent_manager.h"
#include "harness/interfaces.h"

namespace act::harness
{

/// Tool for spawning and managing sub-agents.
/// Delegates to SubagentManager for lifecycle management.
class SubagentTool : public ITool
{
public:
    explicit SubagentTool(act::framework::SubagentManager &manager);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    [[nodiscard]] static std::optional<act::framework::SubagentType>
    parseSubagentType(const QString &str);

    act::framework::SubagentManager &m_manager;
};

} // namespace act::harness
