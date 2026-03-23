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

    // Persistence
    bool load();
    bool save();

    [[nodiscard]] QString configFilePath() const;

    // Defaults
    static constexpr const char *DEFAULT_MODEL = "claude-sonnet-4-20250514";
    static constexpr const char *DEFAULT_PROVIDER = "anthropic";

private:
    QString m_workspacePath;
    QString m_model = QString::fromUtf8(DEFAULT_MODEL);
    QMap<QString, QString> m_apiKeys;
};

} // namespace act::services
