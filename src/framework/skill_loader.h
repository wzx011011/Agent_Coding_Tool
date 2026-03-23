#pragma once

#include <QDir>
#include <QList>
#include <QString>

#include "framework/skill_catalog.h"

namespace act::framework
{

/// Loads skill definitions from the filesystem.
/// Skills are TOML files in the skill directory:
///   - Each skill file: `{name}.toml` or `{name}/skill.toml`
///   - Fields: name, version, description, system_prompt, body, triggers, metadata
class SkillLoader
{
public:
    /// Load all skills from the given directory into the provided catalog.
    /// Returns the number of skills loaded (skips invalid files).
    [[nodiscard]] int loadFromDirectory(const QString &skillDir, SkillCatalog &catalog);

    /// Load skills from a list of directories into the provided catalog.
    [[nodiscard]] int loadFromDirectories(const QStringList &dirs, SkillCatalog &catalog);

    /// Get the list of directories that were searched.
    [[nodiscard]] QStringList searchedDirectories() const { return m_searchedDirs; }

    /// Get errors encountered during loading.
    [[nodiscard]] QStringList errors() const { return m_errors; }

private:
    [[nodiscard]] bool loadSkillFile(const QString &filePath,
                                       SkillCatalog &catalog);
    [[nodiscard]] bool parseSkillToml(const QString &content,
                                       const QString &filePath,
                                       SkillEntry &entry,
                                       QStringList &errors) const;

    QStringList m_searchedDirs;
    QStringList m_errors;
};

} // namespace act::framework
