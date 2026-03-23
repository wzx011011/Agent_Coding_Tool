#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace act::framework
{

/// Metadata describing a skill.
struct SkillEntry
{
    QString name;        // Unique skill identifier (e.g. "commit", "review-pr")
    QString version;     // Skill version
    QString description; // Human-readable description
    QString systemPrompt; // Injected into system message (always present)
    QString body;        // Injected on demand as tool_result content
    QStringList triggers; // Keywords that activate this skill
    QJsonObject metadata;  // Extension data
};

/// Registry of available skills.
class SkillCatalog
{
public:
    /// Register a skill. Returns false if a skill with the same name exists.
    [[nodiscard]] bool registerSkill(const SkillEntry &skill);

    /// Unregister a skill by name. Returns false if not found.
    [[nodiscard]] bool unregisterSkill(const QString &name);

    /// Look up a skill by name.
    [[nodiscard]] const SkillEntry *findSkill(const QString &name) const;

    /// Find skills matching any of the given trigger keywords.
    [[nodiscard]] QList<const SkillEntry *> findByTriggers(const QStringList &keywords) const;

    /// List all registered skill names.
    [[nodiscard]] QStringList listSkills() const;

    /// Number of registered skills.
    [[nodiscard]] int size() const { return m_skills.size(); }

    /// Build a combined system prompt from all registered skills' system_prompt fields.
    /// Each skill's system prompt is appended as a separate section.
    [[nodiscard]] QString buildSystemPrompt() const;

    /// Get the body content for a specific skill.
    [[nodiscard]] QString skillBody(const QString &name) const;

private:
    QMap<QString, SkillEntry> m_skills;
};

} // namespace act::framework
