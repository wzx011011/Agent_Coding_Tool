#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "services/config_manager.h"

using namespace act::services;

class ConfigManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_tempDir.emplace();
        m_config = std::make_unique<ConfigManager>(m_tempDir->path());
    }

    QTemporaryDir *tempDir() { return &(*m_tempDir); }
    ConfigManager *config() { return m_config.get(); }

private:
    std::optional<QTemporaryDir> m_tempDir;
    std::unique_ptr<ConfigManager> m_config;
};

TEST_F(ConfigManagerTest, DefaultsToClaudeModel)
{
    EXPECT_EQ(config()->currentModel(), ConfigManager::DEFAULT_MODEL);
}

TEST_F(ConfigManagerTest, CanSetModel)
{
    config()->setModel(QStringLiteral("claude-opus-4-20250514"));
    EXPECT_EQ(config()->currentModel(), QStringLiteral("claude-opus-4-20250514"));
}

TEST_F(ConfigManagerTest, CanSetAndGetApiKey)
{
    config()->setApiKey(QStringLiteral("anthropic"), QStringLiteral("sk-test-123"));
    EXPECT_EQ(config()->apiKey(QStringLiteral("anthropic")),
              QStringLiteral("sk-test-123"));
}

TEST_F(ConfigManagerTest, ApiKeyIsCaseInsensitive)
{
    config()->setApiKey(QStringLiteral("Anthropic"), QStringLiteral("sk-test"));
    EXPECT_EQ(config()->apiKey(QStringLiteral("ANTHROPIC")),
              QStringLiteral("sk-test"));
}

TEST_F(ConfigManagerTest, MissingApiKeyReturnsEmpty)
{
    EXPECT_TRUE(config()->apiKey(QStringLiteral("anthropic")).isEmpty());
}

TEST_F(ConfigManagerTest, WorkspacePath)
{
    EXPECT_EQ(config()->workspacePath(), tempDir()->path());
}

TEST_F(ConfigManagerTest, SaveAndLoadRoundtrip)
{
    config()->setModel(QStringLiteral("custom-model"));
    config()->setApiKey(QStringLiteral("anthropic"), QStringLiteral("sk-roundtrip"));

    ASSERT_TRUE(config()->save());

    // Create a new ConfigManager to verify persistence
    ConfigManager loaded(tempDir()->path());
    ASSERT_TRUE(loaded.load());

    EXPECT_EQ(loaded.currentModel(), QStringLiteral("custom-model"));
    EXPECT_EQ(loaded.apiKey(QStringLiteral("anthropic")),
              QStringLiteral("sk-roundtrip"));
}

TEST_F(ConfigManagerTest, LoadNonexistentFileUsesDefaults)
{
    ASSERT_TRUE(config()->load());
    EXPECT_EQ(config()->currentModel(), ConfigManager::DEFAULT_MODEL);
}

TEST_F(ConfigManagerTest, SaveCreatesConfigDirectory)
{
    config()->setModel(QStringLiteral("test-model"));
    ASSERT_TRUE(config()->save());

    QFile file(QDir(tempDir()->path())
                   .absoluteFilePath(QStringLiteral(".act/config.toml")));
    EXPECT_TRUE(file.exists());
}

// --- Provider & Network Config ---

TEST_F(ConfigManagerTest, DefaultProviderIsAnthropic)
{
    EXPECT_EQ(config()->provider(), ConfigManager::DEFAULT_PROVIDER);
}

TEST_F(ConfigManagerTest, CanSetProvider)
{
    config()->setProvider(QStringLiteral("openai_compat"));
    EXPECT_EQ(config()->provider(), QStringLiteral("openai_compat"));
}

TEST_F(ConfigManagerTest, ProviderIsCaseInsensitive)
{
    config()->setProvider(QStringLiteral("OpenAI_Compat"));
    EXPECT_EQ(config()->provider(), QStringLiteral("openai_compat"));
}

TEST_F(ConfigManagerTest, DefaultBaseUrlForAnthropic)
{
    EXPECT_EQ(config()->baseUrl(), QString(ConfigManager::ANTHROPIC_BASE_URL));
}

TEST_F(ConfigManagerTest, DefaultBaseUrlForOpenAICompat)
{
    config()->setProvider(QStringLiteral("openai_compat"));
    EXPECT_EQ(config()->baseUrl(), QString(ConfigManager::OPENAI_COMPAT_BASE_URL));
}

TEST_F(ConfigManagerTest, CanSetCustomBaseUrl)
{
    config()->setBaseUrl(QStringLiteral("https://custom.api.com/v1"));
    EXPECT_EQ(config()->baseUrl(), QStringLiteral("https://custom.api.com/v1"));
}

TEST_F(ConfigManagerTest, CanSetProxy)
{
    config()->setProxy(QStringLiteral("http://127.0.0.1:7890"));
    EXPECT_EQ(config()->proxy(), QStringLiteral("http://127.0.0.1:7890"));
}

TEST_F(ConfigManagerTest, DefaultProxyIsEmpty)
{
    EXPECT_TRUE(config()->proxy().isEmpty());
}

TEST_F(ConfigManagerTest, ProviderAndNetworkRoundtrip)
{
    config()->setModel(QStringLiteral("glm-4"));
    config()->setProvider(QStringLiteral("openai_compat"));
    config()->setBaseUrl(QStringLiteral("https://custom.api.com"));
    config()->setProxy(QStringLiteral("http://proxy:8080"));
    config()->setApiKey(QStringLiteral("openai_compat"), QStringLiteral("key-123"));

    ASSERT_TRUE(config()->save());

    ConfigManager loaded(tempDir()->path());
    ASSERT_TRUE(loaded.load());

    EXPECT_EQ(loaded.provider(), QStringLiteral("openai_compat"));
    EXPECT_EQ(loaded.baseUrl(), QStringLiteral("https://custom.api.com"));
    EXPECT_EQ(loaded.proxy(), QStringLiteral("http://proxy:8080"));
    EXPECT_EQ(loaded.apiKey(QStringLiteral("openai_compat")), QStringLiteral("key-123"));
}
