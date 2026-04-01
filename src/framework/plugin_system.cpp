#include "framework/plugin_system.h"

#include <QDir>
#include <QFile>

#include <spdlog/spdlog.h>

#include <toml++/toml.hpp>

namespace act::framework {

PluginSystem::PluginSystem(const QString &pluginDir)
    : m_pluginDir(QDir::cleanPath(pluginDir))
{
}

QList<PluginManifest> PluginSystem::discoverPlugins() const
{
    QList<PluginManifest> found;

    QDir dir(m_pluginDir);
    if (!dir.exists())
    {
        spdlog::info("PluginSystem: plugin directory does not exist: {}",
                     m_pluginDir.toStdString());
        return found;
    }

    // Scan subdirectories for plugin.toml
    for (const auto &entry :
         dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QString subDir = dir.absoluteFilePath(entry.fileName());
        QString pluginFile = findPluginFile(subDir);
        if (!pluginFile.isEmpty())
        {
            PluginManifest manifest = parseManifest(subDir);
            if (!manifest.name.isEmpty())
            {
                found.append(manifest);
            }
        }
    }

    spdlog::info("PluginSystem: discovered {} plugins in {}",
                 found.size(), m_pluginDir.toStdString());
    return found;
}

bool PluginSystem::loadPlugin(const QString &name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QDir dir(m_pluginDir);
    QString subDir = dir.absoluteFilePath(name);
    QString pluginFile = findPluginFile(subDir);
    if (pluginFile.isEmpty())
    {
        spdlog::warn("PluginSystem: no plugin.toml found for '{}'",
                     name.toStdString());
        return false;
    }

    PluginManifest manifest = parseManifest(subDir);
    if (manifest.name.isEmpty())
    {
        spdlog::warn("PluginSystem: failed to parse manifest for '{}'",
                     name.toStdString());
        return false;
    }

    m_plugins.insert(name, manifest);
    spdlog::info("PluginSystem: loaded plugin '{}' v{}",
                 manifest.name.toStdString(),
                 manifest.version.toStdString());
    return true;
}

std::optional<PluginManifest> PluginSystem::getPlugin(
    const QString &name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_plugins.constFind(name);
    if (it != m_plugins.constEnd())
        return it.value();
    return std::nullopt;
}

QStringList PluginSystem::loadedPlugins() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_plugins.keys();
}

bool PluginSystem::unloadPlugin(const QString &name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_plugins.contains(name))
        return false;

    m_plugins.remove(name);
    spdlog::info("PluginSystem: unloaded plugin '{}'", name.toStdString());
    return true;
}

PluginManifest PluginSystem::parseManifest(const QString &dir) const
{
    PluginManifest manifest;

    QString pluginFile = findPluginFile(dir);
    if (pluginFile.isEmpty())
        return manifest;

    QFile file(pluginFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        spdlog::warn("PluginSystem: cannot read plugin file: {}",
                     pluginFile.toStdString());
        return manifest;
    }

    QByteArray data = file.readAll();

    try
    {
        auto tbl = toml::parse(data.toStdString());

        // Parse [plugin] table
        auto pluginView = tbl["plugin"];
        if (!pluginView.is_table())
        {
            spdlog::warn("PluginSystem: missing [plugin] table in {}",
                         pluginFile.toStdString());
            return manifest;
        }

        auto &pluginTbl = *pluginView.as_table();

        // name (required)
        {
            auto nv = pluginTbl["name"];
            if (nv.is_string())
            {
                auto sv = nv.value_or(std::string_view{});
                manifest.name =
                    QString::fromUtf8(sv.data(), static_cast<int>(sv.size()));
            }
        }

        if (manifest.name.isEmpty())
        {
            spdlog::warn("PluginSystem: missing 'name' in {}",
                         pluginFile.toStdString());
            return manifest;
        }

        // version (optional, default "0.1.0")
        {
            auto nv = pluginTbl["version"];
            auto sv = nv.value_or(std::string_view{});
            manifest.version = QString::fromUtf8(sv.data(),
                                                 static_cast<int>(sv.size()));
            if (manifest.version.isEmpty())
                manifest.version = QStringLiteral("0.1.0");
        }

        // type (optional, default "skill")
        {
            auto nv = pluginTbl["type"];
            auto sv = nv.value_or(std::string_view{});
            QString typeStr = QString::fromUtf8(sv.data(),
                                                static_cast<int>(sv.size()));
            if (typeStr == QStringLiteral("agent"))
                manifest.type = PluginType::Agent;
            else if (typeStr == QStringLiteral("hook"))
                manifest.type = PluginType::Hook;
            else if (typeStr == QStringLiteral("mcp"))
                manifest.type = PluginType::MCP;
            else
                manifest.type = PluginType::Skill;
        }

        // description (optional)
        {
            auto nv = pluginTbl["description"];
            auto sv = nv.value_or(std::string_view{});
            manifest.description = QString::fromUtf8(sv.data(),
                                                     static_cast<int>(sv.size()));
        }

        // entry_point (optional)
        {
            auto nv = pluginTbl["entry_point"];
            auto sv = nv.value_or(std::string_view{});
            manifest.entryPoint = QString::fromUtf8(sv.data(),
                                                    static_cast<int>(sv.size()));
        }

        // dependencies (optional array of strings)
        {
            auto depsView = pluginTbl["dependencies"];
            if (depsView.is_array())
            {
                auto &arr = *depsView.as_array();
                for (size_t i = 0; i < arr.size(); ++i)
                {
                    if (arr[i].is_string())
                    {
                        auto sv = arr[i].value_or(std::string_view{});
                        manifest.dependencies.append(
                            QString::fromUtf8(sv.data(),
                                              static_cast<int>(sv.size())));
                    }
                }
            }
        }

        // config (optional table with string values)
        {
            auto configView = pluginTbl["config"];
            if (configView.is_table())
            {
                for (const auto &[k, v] : *configView.as_table())
                {
                    auto ks = std::string_view(k);
                    QString qKey = QString::fromUtf8(ks.data(),
                                                     static_cast<int>(ks.size()));
                    auto sv = v.value_or(std::string_view{});
                    manifest.config[qKey] =
                        QString::fromUtf8(sv.data(),
                                          static_cast<int>(sv.size()));
                }
            }
        }
    }
    catch (const toml::parse_error &e)
    {
        spdlog::warn("PluginSystem: TOML parse error in {}: {}",
                     pluginFile.toStdString(), e.what());
    }

    return manifest;
}

QString PluginSystem::findPluginFile(const QString &dir)
{
    QString path = QDir(dir).absoluteFilePath(QStringLiteral("plugin.toml"));
    if (QFile::exists(path))
        return path;
    return {};
}

} // namespace act::framework
