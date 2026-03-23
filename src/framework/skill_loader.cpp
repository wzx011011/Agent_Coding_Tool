#include "framework/skill_loader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include <toml++/toml.hpp>

namespace act::framework
{

int SkillLoader::loadFromDirectory(const QString &skillDir, SkillCatalog &catalog)
{
    m_searchedDirs.append(QDir::cleanPath(skillDir));

    QDir dir(skillDir);
    if (!dir.exists())
    {
        m_errors.append(QStringLiteral("Skill directory not found: %1").arg(skillDir));
        return 0;
    }

    int loaded = 0;

    // Look for skill files: name.toml or name/skill.toml
    for (const auto &entry : dir.entryInfoList(QDir::Files | QDir::Dirs))
    {
        QString fullPath = dir.absoluteFilePath(entry.fileName());

        if (entry.isFile())
        {
            if (entry.suffix() == QLatin1String("toml"))
            {
                if (loadSkillFile(fullPath, catalog))
                    ++loaded;
            }
        }
        else if (entry.isDir())
        {
            // Check for skill.toml inside subdirectory
            QString subSkill = dir.absoluteFilePath(
                entry.fileName() + QLatin1String("/skill.toml"));
            if (QFile::exists(subSkill))
            {
                if (loadSkillFile(subSkill, catalog))
                    ++loaded;
            }
        }
    }

    return loaded;
}

int SkillLoader::loadFromDirectories(const QStringList &dirs, SkillCatalog &catalog)
{
    int total = 0;
    for (const auto &dir : dirs)
    {
        total += loadFromDirectory(dir, catalog);
    }
    return total;
}

bool SkillLoader::loadSkillFile(const QString &filePath,
                                   SkillCatalog &catalog)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_errors.append(QStringLiteral("Cannot read skill file: %1").arg(filePath));
        return false;
    }

    QByteArray data = file.readAll();
    SkillEntry entry;
    QStringList parseErrors;
    if (!parseSkillToml(QString::fromUtf8(data), filePath, entry, parseErrors))
    {
        m_errors.append(parseErrors);
        return false;
    }

    if (!catalog.registerSkill(entry))
    {
        m_errors.append(QStringLiteral("Duplicate skill name '%1' in: %2")
                             .arg(entry.name, filePath));
        return false;
    }

    spdlog::info("Loaded skill '{}' from {}", entry.name.toStdString(),
                  filePath.toStdString());
    return true;
}

static QString nodeViewToQString(const toml::node_view<toml::node> &nv)
{
    auto sv = nv.value_or(std::string_view{});
    return QString::fromUtf8(sv.data(), static_cast<int>(sv.size()));
}

bool SkillLoader::parseSkillToml(const QString &content,
                                  const QString &filePath,
                                  SkillEntry &entry,
                                  QStringList &errors) const
{
    try
    {
        auto tbl = toml::parse(content.toStdString());

        // name (required)
        auto nameView = tbl["name"];
        if (!nameView.is_string())
        {
            errors.append(QStringLiteral("Skill missing 'name' in: %1")
                              .arg(filePath));
            return false;
        }
        entry.name = nodeViewToQString(nameView);

        // version (optional, default "0.1.0")
        entry.version = nodeViewToQString(tbl["version"]);
        if (entry.version.isEmpty())
            entry.version = QStringLiteral("0.1.0");

        // description (optional)
        entry.description = nodeViewToQString(tbl["description"]);

        // system_prompt (optional)
        entry.systemPrompt = nodeViewToQString(tbl["system_prompt"]);

        // body (optional)
        entry.body = nodeViewToQString(tbl["body"]);

        // triggers (optional array of strings)
        auto triggersView = tbl["triggers"];
        if (triggersView.is_array())
        {
            auto &arr = *triggersView.as_array();
            for (size_t i = 0; i < arr.size(); ++i)
            {
                if (arr[i].is_string())
                {
                    auto sv = arr[i].value_or(std::string_view{});
                    entry.triggers.append(
                        QString::fromUtf8(sv.data(), static_cast<int>(sv.size())));
                }
            }
        }

        // metadata (optional table with key-value pairs)
        auto metadataView = tbl["metadata"];
        if (metadataView.is_table())
        {
            QJsonObject metaObj;
            for (const auto &[k, v] : *metadataView.as_table())
            {
                auto ks = std::string_view(k);
                QString qKey = QString::fromUtf8(ks.data(), static_cast<int>(ks.size()));

                if (v.is_string())
                {
                    auto sv = v.value_or(std::string_view{});
                    metaObj[qKey] = QString::fromUtf8(sv.data(), static_cast<int>(sv.size()));
                }
                else if (v.is_integer())
                {
                    metaObj[qKey] = static_cast<qint64>(v.as_integer()->get());
                }
                else if (v.is_boolean())
                {
                    metaObj[qKey] = v.as_boolean()->get();
                }
                else if (v.is_array())
                {
                    QJsonArray jsonArr;
                    auto &arr = *v.as_array();
                    for (size_t i = 0; i < arr.size(); ++i)
                    {
                        if (arr[i].is_string())
                        {
                            auto sv = arr[i].value_or(std::string_view{});
                            jsonArr.append(
                                QString::fromUtf8(sv.data(), static_cast<int>(sv.size())));
                        }
                    }
                    metaObj[qKey] = jsonArr;
                }
            }
            entry.metadata = metaObj;
        }

        return true;
    }
    catch (const toml::parse_error &e)
    {
        errors.append(QStringLiteral("TOML parse error in %1: %2")
                          .arg(filePath)
                          .arg(QString::fromUtf8(e.what())));
        return false;
    }
}

} // namespace act::framework
