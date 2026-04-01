#pragma once

#include <QDir>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <mutex>
#include <optional>

#include "framework/plugin_types.h"

namespace act::framework {

/// Discovers and loads plugins from a directory.
/// Plugins are subdirectories containing a plugin.toml manifest file.
/// Directory structure:
///   .act/plugins/<name>/plugin.toml  (manifest)
///   .act/plugins/<name>/skill.toml   (skill definition, if type=Skill)
class PluginSystem {
public:
    explicit PluginSystem(const QString &pluginDir);
    ~PluginSystem() = default;

    /// Discover all plugins in the plugin directory.
    /// Scans subdirectories for plugin.toml files.
    QList<PluginManifest> discoverPlugins() const;

    /// Load a specific plugin by name.
    /// Returns true if the plugin was found and loaded successfully.
    bool loadPlugin(const QString &name);

    /// Get a loaded plugin manifest by name.
    std::optional<PluginManifest> getPlugin(const QString &name) const;

    /// List all loaded plugin names.
    QStringList loadedPlugins() const;

    /// Unload a plugin by name. Returns true if the plugin was loaded.
    bool unloadPlugin(const QString &name);

private:
    /// Parse a plugin.toml manifest from the given directory.
    PluginManifest parseManifest(const QString &dir) const;

    /// Find the plugin.toml file in a directory.
    static QString findPluginFile(const QString &dir);

    QString m_pluginDir;
    QMap<QString, PluginManifest> m_plugins;
    mutable std::mutex m_mutex;
};

} // namespace act::framework
