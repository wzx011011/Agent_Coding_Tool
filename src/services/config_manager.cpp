#include "services/config_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <spdlog/spdlog.h>

namespace act::services
{

ConfigManager::ConfigManager()
    : m_configFilePath(defaultConfigPath())
{
}

ConfigManager::ConfigManager(const QString &configPath)
    : m_configFilePath(configPath)
{
}

QString ConfigManager::defaultConfigDir()
{
    return QDir::homePath() + QStringLiteral("/.act");
}

QString ConfigManager::defaultConfigPath()
{
    return QDir::homePath() + QStringLiteral("/.act/settings.json");
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

QString ConfigManager::configFilePath() const
{
    return m_configFilePath;
}

bool ConfigManager::isConfigured() const
{
    if (m_provider.isEmpty())
        return false;
    if (m_apiKeys.value(m_provider.toLower()).isEmpty())
        return false;
    return true;
}

QString ConfigManager::provider() const
{
    return m_provider;
}

void ConfigManager::setProvider(const QString &provider)
{
    m_provider = provider.toLower();
}

QString ConfigManager::baseUrl() const
{
    if (!m_baseUrl.isEmpty())
        return m_baseUrl;
    return defaultBaseUrl(m_provider);
}

void ConfigManager::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
}

QString ConfigManager::proxy() const
{
    return m_proxy;
}

void ConfigManager::setProxy(const QString &proxy)
{
    m_proxy = proxy;
}

QString ConfigManager::defaultBaseUrl(const QString &provider)
{
    if (provider.toLower() == QStringLiteral("anthropic"))
        return QString::fromUtf8(ANTHROPIC_BASE_URL);
    return QString::fromUtf8(OPENAI_COMPAT_BASE_URL);
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

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        spdlog::error("Cannot open config file: {}", path.toStdString());
        return false;
    }

    const auto data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        spdlog::error("Failed to parse config JSON: {}",
                      parseError.errorString().toStdString());
        return false;
    }

    if (!doc.isObject())
    {
        spdlog::error("Config root must be a JSON object");
        return false;
    }

    const auto root = doc.object();

    // Read model
    if (root.contains(QStringLiteral("model")))
    {
        const auto val = root[QStringLiteral("model")];
        if (val.isString())
            m_model = val.toString();
    }

    // Read provider
    if (root.contains(QStringLiteral("provider")))
    {
        const auto val = root[QStringLiteral("provider")];
        if (val.isString())
            m_provider = val.toString().toLower();
    }

    // Read [network] section
    if (root.contains(QStringLiteral("network")))
    {
        const auto network = root[QStringLiteral("network")].toObject();
        if (network.contains(QStringLiteral("base_url")))
        {
            const auto val = network[QStringLiteral("base_url")];
            if (val.isString())
                m_baseUrl = val.toString();
        }
        if (network.contains(QStringLiteral("proxy")))
        {
            const auto val = network[QStringLiteral("proxy")];
            if (val.isString())
                m_proxy = val.toString();
        }
        if (network.contains(QStringLiteral("fallback_providers")))
        {
            const auto val = network[QStringLiteral("fallback_providers")];
            if (val.isArray())
            {
                const auto arr = val.toArray();
                for (const auto &item : arr)
                {
                    if (item.isString())
                        m_fallbackProviders.append(item.toString());
                }
            }
        }
    }

    // Read API keys
    if (root.contains(QStringLiteral("api_keys")))
    {
        const auto keys = root[QStringLiteral("api_keys")].toObject();
        for (auto it = keys.constBegin(); it != keys.constEnd(); ++it)
        {
            const auto entry = it.value().toObject();
            if (entry.contains(QStringLiteral("key")))
            {
                const auto keyVal = entry[QStringLiteral("key")];
                if (keyVal.isString())
                    m_apiKeys[it.key().toLower()] = keyVal.toString();
            }
        }
    }

    spdlog::info("Config loaded from {}", path.toStdString());
    return true;
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

    QJsonObject root;
    root[QStringLiteral("provider")] = m_provider;
    root[QStringLiteral("model")] = m_model;

    QJsonObject network;
    if (!m_baseUrl.isEmpty())
        network[QStringLiteral("base_url")] = m_baseUrl;
    if (!m_proxy.isEmpty())
        network[QStringLiteral("proxy")] = m_proxy;
    if (!m_fallbackProviders.isEmpty())
    {
        QJsonArray fallbackArr;
        for (const auto &p : m_fallbackProviders)
            fallbackArr.append(p);
        network[QStringLiteral("fallback_providers")] = fallbackArr;
    }
    if (!network.isEmpty())
        root[QStringLiteral("network")] = network;

    QJsonObject keys;
    for (auto it = m_apiKeys.constBegin(); it != m_apiKeys.constEnd(); ++it)
    {
        QJsonObject entry;
        entry[QStringLiteral("key")] = it.value();
        keys[it.key()] = entry;
    }
    if (!keys.isEmpty())
        root[QStringLiteral("api_keys")] = keys;

    QJsonDocument doc(root);
    const auto json = doc.toJson(QJsonDocument::Indented);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        spdlog::error("Cannot write config file: {}", path.toStdString());
        return false;
    }
    file.write(json);
    file.close();

    spdlog::info("Config saved to {}", path.toStdString());
    return true;
}

} // namespace act::services
