#include "services/config_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <sstream>

#include <spdlog/spdlog.h>

#include <toml++/toml.hpp>

namespace act::services
{

ConfigManager::ConfigManager(const QString &workspacePath)
    : m_workspacePath(workspacePath)
{
}

QString ConfigManager::currentModel() const
{
    return m_model;
}

void ConfigManager::setModel(const QString &model)
{
    m_model = model;
}

QString ConfigManager::apiKey(const QString &provider) const
{
    return m_apiKeys.value(provider.toLower());
}

void ConfigManager::setApiKey(const QString &provider, const QString &key)
{
    m_apiKeys[provider.toLower()] = key;
}

QString ConfigManager::workspacePath() const
{
    return m_workspacePath;
}

QString ConfigManager::configFilePath() const
{
    return QDir(m_workspacePath).absoluteFilePath(QStringLiteral(".act/config.toml"));
}

bool ConfigManager::load()
{
    const auto path = configFilePath();
    QFileInfo fi(path);
    if (!fi.exists())
    {
        spdlog::info("Config file not found at {}, using defaults",
                     path.toStdString());
        return true;
    }

    try
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            spdlog::error("Cannot open config file: {}", path.toStdString());
            return false;
        }
        const auto data = file.readAll();
        file.close();

        auto tbl = toml::parse(data.toStdString());

        // Read model
        auto modelNode = tbl["model"];
        if (modelNode.is_string())
        {
            auto val = modelNode.value_or(std::string_view{DEFAULT_MODEL});
            m_model = QString::fromUtf8(val.data(), static_cast<int>(val.size()));
        }

        // Read API keys
        auto keysNode = tbl["api_keys"];
        if (keysNode.is_table())
        {
            for (const auto &[k, v] : *keysNode.as_table())
            {
                if (v.is_table())
                {
                    auto keyVal = v.as_table()->get("key");
                    if (keyVal && keyVal->is_string())
                    {
                        auto sv = keyVal->value_or(std::string_view{});
                        m_apiKeys[QString::fromStdString(std::string(k))] =
                            QString::fromUtf8(sv.data(),
                                              static_cast<int>(sv.size()));
                    }
                }
            }
        }

        spdlog::info("Config loaded from {}", path.toStdString());
        return true;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Failed to parse config: {}", e.what());
        return false;
    }
}

bool ConfigManager::save()
{
    const auto path = configFilePath();
    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
    {
        spdlog::error("Cannot create config directory: {}",
                      dir.absolutePath().toStdString());
        return false;
    }

    try
    {
        toml::table config;
        config.insert("model", m_model.toStdString());

        toml::table keys;
        for (auto it = m_apiKeys.constBegin(); it != m_apiKeys.constEnd(); ++it)
        {
            toml::table entry;
            entry.insert("key", it.value().toStdString());
            keys.insert(it.key().toStdString(), std::move(entry));
        }
        config.insert("api_keys", std::move(keys));

        std::ostringstream ss;
        ss << config;

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            spdlog::error("Cannot write config file: {}", path.toStdString());
            return false;
        }
        file.write(QByteArray::fromStdString(ss.str()));
        file.close();

        spdlog::info("Config saved to {}", path.toStdString());
        return true;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Failed to save config: {}", e.what());
        return false;
    }
}

} // namespace act::services
