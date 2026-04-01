#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

namespace act::framework {

/// Type of plugin.
enum class PluginType {
    Skill,   // TOML skill definition
    Agent,   // Custom sub-agent template
    Hook,    // Pre-defined hook set
    MCP      // MCP server config
};

/// Metadata describing a plugin, parsed from plugin.toml.
struct PluginManifest {
    QString name;
    QString version;
    PluginType type = PluginType::Skill;
    QString description;
    QString entryPoint;            // File path relative to plugin dir
    QMap<QString, QString> config; // Plugin-specific config
    QStringList dependencies;      // Other plugin names
};

} // namespace act::framework
