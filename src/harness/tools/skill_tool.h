#pragma once

#include <QJsonObject>
#include <QString>

#include "framework/skill_catalog.h"
#include "harness/interfaces.h"

namespace act::harness
{

/// Tool for loading and inspecting skills from the SkillCatalog.
/// Allows the agent to discover available skills and retrieve their content.
class SkillTool : public ITool
{
public:
    explicit SkillTool(act::framework::SkillCatalog &catalog);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    act::framework::SkillCatalog &m_catalog;
};

} // namespace act::harness
