#include "services/config_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <spdlog/spdlog.h>

namespace act::services {

ConfigManager::ConfigManager() : m_configFilePath(defaultConfigPath()) {
}

ConfigManager::ConfigManager(const QString &configPath) : m_configFilePath(configPath) {
}

QString ConfigManager::defaultConfigDir() {
    return QDir::homePath() + QStringLiteral("/.act");
}

QString ConfigManager::defaultConfigPath() {
    return QDir::homePath() + QStringLiteral("/.act/settings.json");
}

QString ConfigManager::currentModel() const {
    return m_model;
}

void ConfigManager::setModel(const QString &model) {
    m_model = model.trimmed();
    if (m_model.isEmpty())
        m_model = QString::fromUtf8(DEFAULT_MODEL);
}

QString ConfigManager::apiKey(const QString &provider) const {
    return m_apiKeys.value(provider.toLower());
}

void ConfigManager::setApiKey(const QString &provider, const QString &key) {
    const auto normalizedProvider = provider.trimmed().toLower();
    if (normalizedProvider.isEmpty())
        return;

    m_apiKeys[normalizedProvider] = key.trimmed();
}

QString ConfigManager::configFilePath() const {
    return m_configFilePath;
}

bool ConfigManager::isConfigured() const {
    if (m_provider.isEmpty())
        return false;
    if (m_apiKeys.value(m_provider.toLower()).isEmpty())
        return false;
    return true;
}

QString ConfigManager::provider() const {
    return m_provider;
}

void ConfigManager::setProvider(const QString &provider) {
    m_provider = provider.trimmed().toLower();
    if (m_provider.isEmpty())
        m_provider = QString::fromUtf8(DEFAULT_PROVIDER);
}

QString ConfigManager::wireApi() const {
    return m_wireApi;
}

void ConfigManager::setWireApi(const QString &wireApi) {
    m_wireApi = wireApi.trimmed().toLower();
    if (m_wireApi.isEmpty())
        m_wireApi = QString::fromUtf8(DEFAULT_WIRE_API);
}

QString ConfigManager::baseUrl() const {
    if (!m_baseUrl.isEmpty())
        return m_baseUrl;
    return defaultBaseUrl(m_provider);
}

void ConfigManager::setBaseUrl(const QString &url) {
    m_baseUrl = url.trimmed();
}

QString ConfigManager::proxy() const {
    return m_proxy;
}

void ConfigManager::setProxy(const QString &proxy) {
    m_proxy = proxy.trimmed();
}

QString ConfigManager::defaultBaseUrl(const QString &provider) {
    if (provider.toLower() == QStringLiteral("anthropic"))
        return QString::fromUtf8(ANTHROPIC_BASE_URL);
    return QString::fromUtf8(OPENAI_COMPAT_BASE_URL);
}

bool ConfigManager::load() {
    const auto path = configFilePath();
    QFileInfo fi(path);
    if (!fi.exists()) {
        spdlog::info("Config file not found at {}, using defaults", path.toStdString());
        return true;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Cannot open config file: {}", path.toStdString());
        return false;
    }

    const auto data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        spdlog::error("Failed to parse config JSON: {}", parseError.errorString().toStdString());
        return false;
    }

    if (!doc.isObject()) {
        spdlog::error("Config root must be a JSON object");
        return false;
    }

    const auto root = doc.object();

    // Read model
    if (root.contains(QStringLiteral("model"))) {
        const auto val = root[QStringLiteral("model")];
        if (val.isString())
            setModel(val.toString());
    }

    // Read provider
    if (root.contains(QStringLiteral("provider"))) {
        const auto val = root[QStringLiteral("provider")];
        if (val.isString())
            setProvider(val.toString());
    }

    // Read [network] section
    if (root.contains(QStringLiteral("network"))) {
        const auto network = root[QStringLiteral("network")].toObject();
        if (network.contains(QStringLiteral("base_url"))) {
            const auto val = network[QStringLiteral("base_url")];
            if (val.isString())
                setBaseUrl(val.toString());
        }
        if (network.contains(QStringLiteral("wire_api"))) {
            const auto val = network[QStringLiteral("wire_api")];
            if (val.isString())
                setWireApi(val.toString());
        }
        if (network.contains(QStringLiteral("proxy"))) {
            const auto val = network[QStringLiteral("proxy")];
            if (val.isString())
                setProxy(val.toString());
        }
        if (network.contains(QStringLiteral("fallback_providers"))) {
            const auto val = network[QStringLiteral("fallback_providers")];
            if (val.isArray()) {
                const auto arr = val.toArray();
                for (const auto &item : arr) {
                    if (item.isString())
                        m_fallbackProviders.append(item.toString());
                }
            }
        }
    }

    // Read API keys
    if (root.contains(QStringLiteral("api_keys"))) {
        const auto keys = root[QStringLiteral("api_keys")].toObject();
        for (auto it = keys.constBegin(); it != keys.constEnd(); ++it) {
            const auto entry = it.value().toObject();
            if (entry.contains(QStringLiteral("key"))) {
                const auto keyVal = entry[QStringLiteral("key")];
                if (keyVal.isString())
                    setApiKey(it.key(), keyVal.toString());
            }
        }
    }

    spdlog::info("Config loaded from {}", path.toStdString());

    // Read active_profile
    if (root.contains(QStringLiteral("active_profile"))) {
        const auto val = root[QStringLiteral("active_profile")];
        if (val.isString())
            m_activeProfile = val.toString();
    }

    // Read profiles
    if (root.contains(QStringLiteral("profiles"))) {
        const auto profiles = root[QStringLiteral("profiles")].toObject();
        for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
            const auto obj = it.value().toObject();
            ModelProfile p;
            p.name = it.key();
            p.model = obj[QStringLiteral("model")].toString();
            p.provider = obj[QStringLiteral("provider")].toString();
            if (p.model.isEmpty() || p.provider.isEmpty()) {
                spdlog::warn("Config: skipping profile '{}' with empty model/provider",
                             p.name.toStdString());
                continue;
            }
            if (obj.contains(QStringLiteral("network"))) {
                const auto net = obj[QStringLiteral("network")].toObject();
                if (net.contains(QStringLiteral("base_url")))
                    p.baseUrl = net[QStringLiteral("base_url")].toString();
                if (net.contains(QStringLiteral("wire_api")))
                    p.wireApi = net[QStringLiteral("wire_api")].toString();
            }
            m_profiles[p.name] = p;
        }
    }

    // Read [feishu] section
    if (root.contains(QStringLiteral("feishu"))) {
        const auto feishu = root[QStringLiteral("feishu")].toObject();
        m_feishuEnabled = feishu.value(QStringLiteral("enabled")).toBool(false);
        m_feishuAppId = feishu.value(QStringLiteral("app_id")).toString();
        m_feishuAppSecret = feishu.value(QStringLiteral("app_secret")).toString();
        m_feishuProxy = feishu.value(QStringLiteral("proxy")).toString();
        m_feishuSessionTimeoutMinutes = feishu.value(
            QStringLiteral("session_timeout_minutes")).toInt(30);
    }

    return true;
}

bool ConfigManager::save() {
    const auto path = configFilePath();
    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        spdlog::error("Cannot create config directory: {}", dir.absolutePath().toStdString());
        return false;
    }

    QJsonObject root;
    root[QStringLiteral("provider")] = m_provider;
    root[QStringLiteral("model")] = m_model;

    QJsonObject network;
    if (!m_baseUrl.isEmpty())
        network[QStringLiteral("base_url")] = m_baseUrl;
    if (!m_wireApi.isEmpty() && m_wireApi != QString::fromUtf8(DEFAULT_WIRE_API)) {
        network[QStringLiteral("wire_api")] = m_wireApi;
    }
    if (!m_proxy.isEmpty())
        network[QStringLiteral("proxy")] = m_proxy;
    if (!m_fallbackProviders.isEmpty()) {
        QJsonArray fallbackArr;
        for (const auto &p : m_fallbackProviders)
            fallbackArr.append(p);
        network[QStringLiteral("fallback_providers")] = fallbackArr;
    }
    if (!network.isEmpty())
        root[QStringLiteral("network")] = network;

    QJsonObject keys;
    for (auto it = m_apiKeys.constBegin(); it != m_apiKeys.constEnd(); ++it) {
        QJsonObject entry;
        entry[QStringLiteral("key")] = it.value();
        keys[it.key()] = entry;
    }
    if (!keys.isEmpty())
        root[QStringLiteral("api_keys")] = keys;

    if (!m_activeProfile.isEmpty())
        root[QStringLiteral("active_profile")] = m_activeProfile;

    if (!m_profiles.isEmpty()) {
        QJsonObject profilesObj;
        for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
            const auto &p = it.value();
            QJsonObject obj;
            obj[QStringLiteral("model")] = p.model;
            obj[QStringLiteral("provider")] = p.provider;
            QJsonObject net;
            if (!p.baseUrl.isEmpty())
                net[QStringLiteral("base_url")] = p.baseUrl;
            if (!p.wireApi.isEmpty())
                net[QStringLiteral("wire_api")] = p.wireApi;
            if (!net.isEmpty())
                obj[QStringLiteral("network")] = net;
            profilesObj[it.key()] = obj;
        }
        root[QStringLiteral("profiles")] = profilesObj;
    }

    // Save [feishu] section
    if (m_feishuEnabled || !m_feishuAppId.isEmpty()) {
        QJsonObject feishu;
        feishu[QStringLiteral("enabled")] = m_feishuEnabled;
        if (!m_feishuAppId.isEmpty())
            feishu[QStringLiteral("app_id")] = m_feishuAppId;
        if (!m_feishuAppSecret.isEmpty())
            feishu[QStringLiteral("app_secret")] = m_feishuAppSecret;
        if (!m_feishuProxy.isEmpty())
            feishu[QStringLiteral("proxy")] = m_feishuProxy;
        if (m_feishuSessionTimeoutMinutes != 30)
            feishu[QStringLiteral("session_timeout_minutes")] = m_feishuSessionTimeoutMinutes;
        root[QStringLiteral("feishu")] = feishu;
    }

    QJsonDocument doc(root);
    const auto json = doc.toJson(QJsonDocument::Indented);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        spdlog::error("Cannot write config file: {}", path.toStdString());
        return false;
    }
    file.write(json);
    file.close();

    spdlog::info("Config saved to {}", path.toStdString());
    return true;
}

bool ConfigManager::setActiveProfile(const QString &profileName) {
    auto it = m_profiles.find(profileName);
    if (it == m_profiles.end())
        return false;

    const auto &p = it.value();
    m_model = p.model;
    m_provider = p.provider.toLower();
    m_baseUrl = p.baseUrl;
    m_wireApi = p.wireApi;
    m_activeProfile = profileName;

    return save();
}

QStringList ConfigManager::profileNames() const {
    return m_profiles.keys();
}

std::optional<ModelProfile> ConfigManager::profile(const QString &name) const {
    auto it = m_profiles.find(name);
    if (it != m_profiles.end())
        return it.value();
    return std::nullopt;
}

bool ConfigManager::hasProfile(const QString &name) const {
    return m_profiles.contains(name);
}

bool ConfigManager::addProfile(const ModelProfile &p) {
    if (p.name.isEmpty() || p.model.isEmpty() || p.provider.isEmpty())
        return false;
    m_profiles[p.name] = p;
    return save();
}

bool ConfigManager::removeProfile(const QString &name) {
    if (!m_profiles.remove(name))
        return false;
    if (m_activeProfile == name)
        m_activeProfile.clear();
    return save();
}

void ConfigManager::setFeishuEnabled(bool enabled) { m_feishuEnabled = enabled; }
void ConfigManager::setFeishuAppId(const QString &appId) { m_feishuAppId = appId.trimmed(); }
void ConfigManager::setFeishuAppSecret(const QString &secret) { m_feishuAppSecret = secret.trimmed(); }
void ConfigManager::setFeishuProxy(const QString &proxy) { m_feishuProxy = proxy.trimmed(); }
void ConfigManager::setFeishuSessionTimeoutMinutes(int minutes) {
    m_feishuSessionTimeoutMinutes = minutes > 0 ? minutes : 30;
}

} // namespace act::services
