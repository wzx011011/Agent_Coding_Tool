#include "harness/tools/skill_tool.h"

#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

SkillTool::SkillTool(act::framework::SkillCatalog &catalog)
    : m_catalog(catalog)
{
}

QString SkillTool::name() const
{
    return QStringLiteral("load_skill");
}

QString SkillTool::description() const
{
    return QStringLiteral("Load and inspect a skill from the skill catalog. "
                          "Returns the skill's system prompt, body content, "
                          "and metadata.");
}

QJsonObject SkillTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("skill_name")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("The name of the skill to load");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("skill_name")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult SkillTool::execute(const QJsonObject &params)
{
    auto skillName = params.value(QStringLiteral("skill_name")).toString();

    if (skillName.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("load_skill requires 'skill_name' parameter"));
    }

    const auto *skill = m_catalog.findSkill(skillName);
    if (!skill)
    {
        return act::core::ToolResult::err(
            act::core::errors::TOOL_NOT_FOUND,
            QStringLiteral("Skill not found: %1. "
                           "Available skills: %2")
                .arg(skillName, m_catalog.listSkills().join(QStringLiteral(", "))));
    }

    spdlog::info("SkillTool: loaded skill '{}'", skillName.toStdString());

    QStringList sections;
    if (!skill->description.isEmpty())
        sections.append(
            QStringLiteral("Description: %1").arg(skill->description));
    if (!skill->version.isEmpty())
        sections.append(
            QStringLiteral("Version: %1").arg(skill->version));
    if (!skill->systemPrompt.isEmpty())
        sections.append(
            QStringLiteral("System Prompt:\n%1").arg(skill->systemPrompt));
    if (!skill->body.isEmpty())
        sections.append(
            QStringLiteral("Body:\n%1").arg(skill->body));
    if (!skill->triggers.isEmpty())
        sections.append(
            QStringLiteral("Triggers: %1")
                .arg(skill->triggers.join(QStringLiteral(", "))));

    if (sections.isEmpty())
        sections.append(QStringLiteral("(skill has no content)"));

    QJsonObject meta;
    meta[QStringLiteral("name")] = skill->name;
    meta[QStringLiteral("version")] = skill->version;
    meta[QStringLiteral("trigger_count")] = skill->triggers.size();

    return act::core::ToolResult::ok(sections.join(QLatin1String("\n\n")),
                                     meta);
}

act::core::PermissionLevel SkillTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool SkillTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
