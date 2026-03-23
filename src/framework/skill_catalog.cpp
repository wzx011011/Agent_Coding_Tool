#include "framework/skill_catalog.h"

#include <algorithm>

namespace act::framework
{

bool SkillCatalog::registerSkill(const SkillEntry &skill)
{
    if (m_skills.contains(skill.name))
        return false;
    m_skills.insert(skill.name, skill);
    return true;
}

bool SkillCatalog::unregisterSkill(const QString &name)
{
    return m_skills.remove(name) > 0;
}

const SkillEntry *SkillCatalog::findSkill(const QString &name) const
{
    auto it = m_skills.constFind(name);
    return it != m_skills.constEnd() ? &it.value() : nullptr;
}

QList<const SkillEntry *> SkillCatalog::findByTriggers(const QStringList &keywords) const
{
    QList<const SkillEntry *> result;
    for (const auto &skill : m_skills)
    {
        for (const auto &trigger : skill.triggers)
        {
            for (const auto &keyword : keywords)
            {
                if (trigger.contains(keyword, Qt::CaseInsensitive))
                {
                    result.append(&skill);
                    break;
                }
            }
        }
    }
    return result;
}

QStringList SkillCatalog::listSkills() const
{
    return m_skills.keys();
}

QString SkillCatalog::buildSystemPrompt() const
{
    QStringList sections;
    for (const auto &skill : m_skills)
    {
        if (!skill.systemPrompt.isEmpty())
        {
            sections.append(
                QStringLiteral("## Skill: %1\n%2")
                    .arg(skill.name, skill.systemPrompt));
        }
    }
    return sections.join(QStringLiteral("\n\n"));
}

QString SkillCatalog::skillBody(const QString &name) const
{
    auto it = m_skills.constFind(name);
    return it != m_skills.constEnd() ? it->body : QString();
}

} // namespace act::framework
