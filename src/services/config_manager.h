#pragma once

#include <QString>

#include "services/interfaces.h"

namespace act::services
{

class ConfigManager : public IConfigManager
{
public:
    explicit ConfigManager(const QString &workspacePath);

    // IConfigManager
    [[nodiscard]] QString currentModel() const override;
    void setModel(const QString &model) override;
    [[nodiscard]] QString apiKey(const QString &provider) const override;
    void setApiKey(const QString &provider, const QString &key) override;
    [[nodiscard]] QString workspacePath() const override;

    // Provider & network
    [[nodiscard]] QString provider() const;
    void setProvider(const QString &provider);
    [[nodiscard]] QString baseUrl() const;
    void setBaseUrl(const QString &url);
    [[nodiscard]] QString proxy() const;
    void setProxy(const QString &proxy);

    // Persistence
    bool load();
    bool save();

    [[nodiscard]] QString configFilePath() const;

    // Defaults
    static constexpr const char *DEFAULT_MODEL = "claude-sonnet-4-20250514";
    static constexpr const char *DEFAULT_PROVIDER = "anthropic";
    static constexpr const char *ANTHROPIC_BASE_URL = "https://api.anthropic.com";
    static constexpr const char *OPENAI_COMPAT_BASE_URL = "https://open.bigmodel.cn/api/v1";

    [[nodiscard]] static QString defaultBaseUrl(const QString &provider);

    // Fallback providers
    [[nodiscard]] QStringList fallbackProviders() const { return m_fallbackProviders; }
    void setFallbackProviders(const QStringList &providers) { m_fallbackProviders = providers; }

private:
    QString m_workspacePath;
    QString m_model = QString::fromUtf8(DEFAULT_MODEL);
    QString m_provider = QString::fromUtf8(DEFAULT_PROVIDER);
    QString m_baseUrl;
    QString m_proxy;
    QMap<QString, QString> m_apiKeys;
    QStringList m_fallbackProviders;
};

} // namespace act::services
