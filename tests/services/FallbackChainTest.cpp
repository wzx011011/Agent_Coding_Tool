#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QJsonObject>

#include "core/error_codes.h"
#include "core/types.h"
#include "services/ai_engine.h"
#include "services/config_manager.h"

using namespace act::core;

/// ConfigManager subclass that allows setting all fields for testing
class TestConfigManager : public act::services::IConfigManager
{
public:
    [[nodiscard]] QString currentModel() const override
    {
        return m_model;
    }
    void setModel(const QString &model) override { m_model = model; }
    [[nodiscard]] QString apiKey(const QString &provider) const override
    {
        return m_apiKeys.value(provider);
    }
    void setApiKey(const QString &provider, const QString &key) override
    {
        m_apiKeys[provider] = key;
    }
    [[nodiscard]] QString configFilePath() const override
    {
        return m_configPath;
    }
    [[nodiscard]] bool isConfigured() const override
    {
        return !m_provider.isEmpty() && m_apiKeys.contains(m_provider);
    }

    // Convenience setters
    void setModelDirect(const QString &m) { m_model = m; }
    void setProviderDirect(const QString &p) { m_provider = p; }
    void addApiKey(const QString &provider, const QString &key)
    {
        m_apiKeys[provider] = key;
    }

    QString m_model = QStringLiteral("test-model");
    QString m_provider = QStringLiteral("anthropic");
    QString m_configPath = QStringLiteral("/tmp/test/settings.json");
    QMap<QString, QString> m_apiKeys;
};

// Verifies that isRetryableError recognizes the expected error codes
TEST(FallbackChainTest, IsRetryableError)
{
    EXPECT_TRUE(act::services::AIEngine::isRetryableError(
        QString::fromStdString(errors::AUTH_ERROR)));
    EXPECT_TRUE(act::services::AIEngine::isRetryableError(
        QString::fromStdString(errors::RATE_LIMIT)));
    EXPECT_TRUE(act::services::AIEngine::isRetryableError(
        QString::fromStdString(errors::PROVIDER_TIMEOUT)));

    EXPECT_FALSE(act::services::AIEngine::isRetryableError(
        QString::fromStdString(errors::NO_PROVIDER)));
    EXPECT_FALSE(act::services::AIEngine::isRetryableError(
        QString::fromStdString(errors::INVALID_PARAMS)));
    EXPECT_FALSE(act::services::AIEngine::isRetryableError(
        QStringLiteral("SOME_OTHER_ERROR")));
}
