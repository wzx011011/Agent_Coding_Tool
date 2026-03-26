#pragma once

#include "services/interfaces.h"
#include <QMap>
#include <QString>
#include <optional>

namespace act::services {

class ConfigManager : public IConfigManager {
  public:
    ConfigManager();
    explicit ConfigManager(const QString &configPath);

    // IConfigManager
    [[nodiscard]] QString currentModel() const override;
    void setModel(const QString &model) override;
    [[nodiscard]] QString apiKey(const QString &provider) const override;
    void setApiKey(const QString &provider, const QString &key) override;
    [[nodiscard]] QString configFilePath() const override;
    [[nodiscard]] bool isConfigured() const override;

    // Provider & network
    [[nodiscard]] QString provider() const;
    void setProvider(const QString &provider);
    [[nodiscard]] QString wireApi() const;
    void setWireApi(const QString &wireApi);
    [[nodiscard]] QString baseUrl() const;
    void setBaseUrl(const QString &url);
    [[nodiscard]] QString proxy() const;
    void setProxy(const QString &proxy);

    // Persistence
    bool load();
    bool save();

    // Defaults
    static constexpr const char *DEFAULT_MODEL = "claude-sonnet-4-20250514";
    static constexpr const char *DEFAULT_PROVIDER = "anthropic";
    static constexpr const char *DEFAULT_WIRE_API = "chat_completions";
    static constexpr const char *ANTHROPIC_BASE_URL = "https://api.anthropic.com";
    static constexpr const char *OPENAI_COMPAT_BASE_URL = "https://open.bigmodel.cn/api/v1";

    [[nodiscard]] static QString defaultConfigDir();
    [[nodiscard]] static QString defaultConfigPath();
    [[nodiscard]] static QString defaultBaseUrl(const QString &provider);

    // Fallback providers
    [[nodiscard]] QStringList fallbackProviders() const { return m_fallbackProviders; }
    void setFallbackProviders(const QStringList &providers) { m_fallbackProviders = providers; }

    // Model profiles
    [[nodiscard]] QString activeProfile() const { return m_activeProfile; }
    [[nodiscard]] bool setActiveProfile(const QString &profileName);
    [[nodiscard]] QStringList profileNames() const;
    [[nodiscard]] std::optional<ModelProfile> profile(const QString &name) const;
    [[nodiscard]] bool hasProfile(const QString &name) const;
    [[nodiscard]] bool addProfile(const ModelProfile &p);
    [[nodiscard]] bool removeProfile(const QString &name);

    // Feishu configuration
    [[nodiscard]] bool feishuEnabled() const { return m_feishuEnabled; }
    [[nodiscard]] QString feishuAppId() const { return m_feishuAppId; }
    [[nodiscard]] QString feishuAppSecret() const { return m_feishuAppSecret; }
    [[nodiscard]] QString feishuProxy() const { return m_feishuProxy; }
    [[nodiscard]] int feishuSessionTimeoutMinutes() const { return m_feishuSessionTimeoutMinutes; }
    void setFeishuEnabled(bool enabled);
    void setFeishuAppId(const QString &appId);
    void setFeishuAppSecret(const QString &secret);
    void setFeishuProxy(const QString &proxy);
    void setFeishuSessionTimeoutMinutes(int minutes);

  private:
    QString m_configFilePath;
    QString m_model = QString::fromUtf8(DEFAULT_MODEL);
    QString m_provider = QString::fromUtf8(DEFAULT_PROVIDER);
    QString m_wireApi = QString::fromUtf8(DEFAULT_WIRE_API);
    QString m_baseUrl;
    QString m_proxy;
    QMap<QString, QString> m_apiKeys;
    QStringList m_fallbackProviders;
    QString m_activeProfile;
    QMap<QString, ModelProfile> m_profiles;

    // Feishu
    bool m_feishuEnabled = false;
    QString m_feishuAppId;
    QString m_feishuAppSecret;
    QString m_feishuProxy;
    int m_feishuSessionTimeoutMinutes = 30;
};

} // namespace act::services
