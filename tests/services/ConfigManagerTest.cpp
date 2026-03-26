#include "services/config_manager.h"
#include <QDir>
#include <QTemporaryDir>
#include <gtest/gtest.h>

using namespace act::services;

class ConfigManagerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_tempDir.emplace();
        m_configPath = m_tempDir->path() + QStringLiteral("/settings.json");
        m_config = std::make_unique<ConfigManager>(m_configPath);
    }

    QTemporaryDir *tempDir() { return &(*m_tempDir); }
    ConfigManager *config() { return m_config.get(); }
    const QString &configPath() const { return m_configPath; }

  private:
    std::optional<QTemporaryDir> m_tempDir;
    QString m_configPath;
    std::unique_ptr<ConfigManager> m_config;
};

TEST_F(ConfigManagerTest, DefaultsToClaudeModel) {
    EXPECT_EQ(config()->currentModel(), ConfigManager::DEFAULT_MODEL);
}

TEST_F(ConfigManagerTest, CanSetModel) {
    config()->setModel(QStringLiteral("claude-opus-4-20250514"));
    EXPECT_EQ(config()->currentModel(), QStringLiteral("claude-opus-4-20250514"));
}

TEST_F(ConfigManagerTest, SetModelTrimsWhitespace) {
    config()->setModel(QStringLiteral("  claude-opus-4-20250514  "));
    EXPECT_EQ(config()->currentModel(), QStringLiteral("claude-opus-4-20250514"));
}

TEST_F(ConfigManagerTest, CanSetAndGetApiKey) {
    config()->setApiKey(QStringLiteral("anthropic"), QStringLiteral("sk-test-123"));
    EXPECT_EQ(config()->apiKey(QStringLiteral("anthropic")), QStringLiteral("sk-test-123"));
}

TEST_F(ConfigManagerTest, ApiKeyIsCaseInsensitive) {
    config()->setApiKey(QStringLiteral("Anthropic"), QStringLiteral("sk-test"));
    EXPECT_EQ(config()->apiKey(QStringLiteral("ANTHROPIC")), QStringLiteral("sk-test"));
}

TEST_F(ConfigManagerTest, MissingApiKeyReturnsEmpty) {
    EXPECT_TRUE(config()->apiKey(QStringLiteral("anthropic")).isEmpty());
}

TEST_F(ConfigManagerTest, ConfigFilePathReturnsConstructorArg) {
    EXPECT_EQ(config()->configFilePath(), configPath());
}

TEST_F(ConfigManagerTest, DefaultConfigDirAndPath) {
    EXPECT_EQ(ConfigManager::defaultConfigDir(), QDir::homePath() + QStringLiteral("/.act"));
    EXPECT_EQ(ConfigManager::defaultConfigPath(), QDir::homePath() + QStringLiteral("/.act/settings.json"));
}

TEST_F(ConfigManagerTest, SaveAndLoadRoundtrip) {
    config()->setModel(QStringLiteral("custom-model"));
    config()->setApiKey(QStringLiteral("anthropic"), QStringLiteral("sk-roundtrip"));

    ASSERT_TRUE(config()->save());

    // Create a new ConfigManager to verify persistence
    ConfigManager loaded(configPath());
    ASSERT_TRUE(loaded.load());

    EXPECT_EQ(loaded.currentModel(), QStringLiteral("custom-model"));
    EXPECT_EQ(loaded.apiKey(QStringLiteral("anthropic")), QStringLiteral("sk-roundtrip"));
}

TEST_F(ConfigManagerTest, LoadNonexistentFileUsesDefaults) {
    ASSERT_TRUE(config()->load());
    EXPECT_EQ(config()->currentModel(), ConfigManager::DEFAULT_MODEL);
}

TEST_F(ConfigManagerTest, SaveCreatesConfigFile) {
    config()->setModel(QStringLiteral("test-model"));
    ASSERT_TRUE(config()->save());

    QFile file(configPath());
    EXPECT_TRUE(file.exists());
}

// --- IsConfigured ---

TEST_F(ConfigManagerTest, NotConfiguredWhenNoApiKey) {
    EXPECT_FALSE(config()->isConfigured());
}

TEST_F(ConfigManagerTest, ConfiguredWithProviderAndApiKey) {
    config()->setProvider(QStringLiteral("anthropic"));
    config()->setApiKey(QStringLiteral("anthropic"), QStringLiteral("sk-test"));
    EXPECT_TRUE(config()->isConfigured());
}

TEST_F(ConfigManagerTest, NotConfiguredWithWrongProviderApiKey) {
    config()->setProvider(QStringLiteral("anthropic"));
    config()->setApiKey(QStringLiteral("openai_compat"), QStringLiteral("sk-test"));
    EXPECT_FALSE(config()->isConfigured());
}

// --- Provider & Network Config ---

TEST_F(ConfigManagerTest, DefaultProviderIsAnthropic) {
    EXPECT_EQ(config()->provider(), ConfigManager::DEFAULT_PROVIDER);
}

TEST_F(ConfigManagerTest, CanSetProvider) {
    config()->setProvider(QStringLiteral("openai_compat"));
    EXPECT_EQ(config()->provider(), QStringLiteral("openai_compat"));
}

TEST_F(ConfigManagerTest, DefaultWireApiIsChatCompletions) {
    EXPECT_EQ(config()->wireApi(), QString(ConfigManager::DEFAULT_WIRE_API));
}

TEST_F(ConfigManagerTest, CanSetWireApi) {
    config()->setWireApi(QStringLiteral("responses"));
    EXPECT_EQ(config()->wireApi(), QStringLiteral("responses"));
}

TEST_F(ConfigManagerTest, ProviderIsCaseInsensitive) {
    config()->setProvider(QStringLiteral("OpenAI_Compat"));
    EXPECT_EQ(config()->provider(), QStringLiteral("openai_compat"));
}

TEST_F(ConfigManagerTest, ProviderTrimsWhitespace) {
    config()->setProvider(QStringLiteral("  OpenAI_Compat  "));
    EXPECT_EQ(config()->provider(), QStringLiteral("openai_compat"));
}

TEST_F(ConfigManagerTest, DefaultBaseUrlForAnthropic) {
    EXPECT_EQ(config()->baseUrl(), QString(ConfigManager::ANTHROPIC_BASE_URL));
}

TEST_F(ConfigManagerTest, DefaultBaseUrlForOpenAICompat) {
    config()->setProvider(QStringLiteral("openai_compat"));
    EXPECT_EQ(config()->baseUrl(), QString(ConfigManager::OPENAI_COMPAT_BASE_URL));
}

TEST_F(ConfigManagerTest, CanSetCustomBaseUrl) {
    config()->setBaseUrl(QStringLiteral("https://custom.api.com/v1"));
    EXPECT_EQ(config()->baseUrl(), QStringLiteral("https://custom.api.com/v1"));
}

TEST_F(ConfigManagerTest, BaseUrlTrimsWhitespace) {
    config()->setBaseUrl(QStringLiteral("  https://custom.api.com/v1  "));
    EXPECT_EQ(config()->baseUrl(), QStringLiteral("https://custom.api.com/v1"));
}

TEST_F(ConfigManagerTest, CanSetProxy) {
    config()->setProxy(QStringLiteral("http://127.0.0.1:7890"));
    EXPECT_EQ(config()->proxy(), QStringLiteral("http://127.0.0.1:7890"));
}

TEST_F(ConfigManagerTest, LoadTrimsPersistedValues) {
    const QByteArray json = R"({
        "provider": "  AixJ  ",
        "model": "  gpt-5.3-codex  ",
        "network": {
            "base_url": "  https://aixj.vip  ",
            "wire_api": " responses ",
            "proxy": "  http://proxy:8080  "
        },
        "api_keys": {
            "AixJ": {
                "key": "  sk-test  "
            }
        }
    })";

    QFile file(configPath());
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(json);
    file.close();

    ASSERT_TRUE(config()->load());
    EXPECT_EQ(config()->provider(), QStringLiteral("aixj"));
    EXPECT_EQ(config()->currentModel(), QStringLiteral("gpt-5.3-codex"));
    EXPECT_EQ(config()->baseUrl(), QStringLiteral("https://aixj.vip"));
    EXPECT_EQ(config()->wireApi(), QStringLiteral("responses"));
    EXPECT_EQ(config()->proxy(), QStringLiteral("http://proxy:8080"));
    EXPECT_EQ(config()->apiKey(QStringLiteral("aixj")), QStringLiteral("sk-test"));
}

TEST_F(ConfigManagerTest, DefaultProxyIsEmpty) {
    EXPECT_TRUE(config()->proxy().isEmpty());
}

TEST_F(ConfigManagerTest, ProviderAndNetworkRoundtrip) {
    config()->setModel(QStringLiteral("glm-4"));
    config()->setProvider(QStringLiteral("openai_compat"));
    config()->setWireApi(QStringLiteral("responses"));
    config()->setBaseUrl(QStringLiteral("https://custom.api.com"));
    config()->setProxy(QStringLiteral("http://proxy:8080"));
    config()->setApiKey(QStringLiteral("openai_compat"), QStringLiteral("key-123"));

    ASSERT_TRUE(config()->save());

    ConfigManager loaded(configPath());
    ASSERT_TRUE(loaded.load());

    EXPECT_EQ(loaded.provider(), QStringLiteral("openai_compat"));
    EXPECT_EQ(loaded.wireApi(), QStringLiteral("responses"));
    EXPECT_EQ(loaded.baseUrl(), QStringLiteral("https://custom.api.com"));
    EXPECT_EQ(loaded.proxy(), QStringLiteral("http://proxy:8080"));
    EXPECT_EQ(loaded.apiKey(QStringLiteral("openai_compat")), QStringLiteral("key-123"));
}

// --- Model Profiles ---

TEST_F(ConfigManagerTest, AddProfileSucceeds) {
    ModelProfile p;
    p.name = QStringLiteral("glm5");
    p.model = QStringLiteral("glm-5-turbo");
    p.provider = QStringLiteral("openai_compat");
    p.baseUrl = QStringLiteral("https://open.bigmodel.cn/api/v1");
    p.wireApi = QStringLiteral("chat_completions");
    ASSERT_TRUE(config()->addProfile(p));
    EXPECT_TRUE(config()->hasProfile(QStringLiteral("glm5")));
}

TEST_F(ConfigManagerTest, AddProfileRejectsEmptyName) {
    ModelProfile p;
    p.name = QStringLiteral("");
    p.model = QStringLiteral("glm-5-turbo");
    p.provider = QStringLiteral("openai_compat");
    EXPECT_FALSE(config()->addProfile(p));
}

TEST_F(ConfigManagerTest, AddProfileRejectsEmptyModel) {
    ModelProfile p;
    p.name = QStringLiteral("test");
    p.model = QStringLiteral("");
    p.provider = QStringLiteral("openai_compat");
    EXPECT_FALSE(config()->addProfile(p));
}

TEST_F(ConfigManagerTest, RemoveProfileSucceeds) {
    ModelProfile p;
    p.name = QStringLiteral("test");
    p.model = QStringLiteral("model1");
    p.provider = QStringLiteral("openai_compat");
    ASSERT_TRUE(config()->addProfile(p));
    ASSERT_TRUE(config()->removeProfile(QStringLiteral("test")));
    EXPECT_FALSE(config()->hasProfile(QStringLiteral("test")));
}

TEST_F(ConfigManagerTest, ProfileNamesReturnsAll) {
    ModelProfile p1;
    p1.name = QStringLiteral("glm5");
    p1.model = QStringLiteral("glm-5-turbo");
    p1.provider = QStringLiteral("openai_compat");

    ModelProfile p2;
    p2.name = QStringLiteral("qwen3");
    p2.model = QStringLiteral("qwen3-max");
    p2.provider = QStringLiteral("openai_compat");

    ASSERT_TRUE(config()->addProfile(p1));
    ASSERT_TRUE(config()->addProfile(p2));

    auto names = config()->profileNames();
    EXPECT_EQ(names.size(), 2);
    EXPECT_TRUE(names.contains(QStringLiteral("glm5")));
    EXPECT_TRUE(names.contains(QStringLiteral("qwen3")));
}

TEST_F(ConfigManagerTest, ProfileLookup) {
    ModelProfile p;
    p.name = QStringLiteral("glm5");
    p.model = QStringLiteral("glm-5-turbo");
    p.provider = QStringLiteral("openai_compat");
    p.baseUrl = QStringLiteral("https://open.bigmodel.cn/api/v1");
    ASSERT_TRUE(config()->addProfile(p));

    auto result = config()->profile(QStringLiteral("glm5"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->model, QStringLiteral("glm-5-turbo"));
    EXPECT_EQ(result->provider, QStringLiteral("openai_compat"));
    EXPECT_EQ(result->baseUrl, QStringLiteral("https://open.bigmodel.cn/api/v1"));
}

TEST_F(ConfigManagerTest, ProfileNotFoundReturnsNullopt) {
    auto result = config()->profile(QStringLiteral("nonexistent"));
    EXPECT_FALSE(result.has_value());
}

TEST_F(ConfigManagerTest, SetActiveProfileUpdatesConfig) {
    ModelProfile p;
    p.name = QStringLiteral("glm5");
    p.model = QStringLiteral("glm-5-turbo");
    p.provider = QStringLiteral("openai_compat");
    p.baseUrl = QStringLiteral("https://open.bigmodel.cn/api/v1");
    p.wireApi = QStringLiteral("chat_completions");
    ASSERT_TRUE(config()->addProfile(p));

    ASSERT_TRUE(config()->setActiveProfile(QStringLiteral("glm5")));

    EXPECT_EQ(config()->activeProfile(), QStringLiteral("glm5"));
    EXPECT_EQ(config()->currentModel(), QStringLiteral("glm-5-turbo"));
    EXPECT_EQ(config()->provider(), QStringLiteral("openai_compat"));
    EXPECT_EQ(config()->baseUrl(), QStringLiteral("https://open.bigmodel.cn/api/v1"));
    EXPECT_EQ(config()->wireApi(), QStringLiteral("chat_completions"));
}

TEST_F(ConfigManagerTest, SetActiveProfileFailsForUnknown) {
    EXPECT_FALSE(config()->setActiveProfile(QStringLiteral("nonexistent")));
}

TEST_F(ConfigManagerTest, ProfileSaveLoadRoundtrip) {
    ModelProfile p1;
    p1.name = QStringLiteral("glm5");
    p1.model = QStringLiteral("glm-5-turbo");
    p1.provider = QStringLiteral("openai_compat");
    p1.baseUrl = QStringLiteral("https://open.bigmodel.cn/api/v1");

    ModelProfile p2;
    p2.name = QStringLiteral("qwen3");
    p2.model = QStringLiteral("qwen3-max");
    p2.provider = QStringLiteral("openai_compat");
    p2.baseUrl = QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1");

    ASSERT_TRUE(config()->addProfile(p1));
    ASSERT_TRUE(config()->addProfile(p2));
    ASSERT_TRUE(config()->setActiveProfile(QStringLiteral("glm5")));

    // Reload from file
    ConfigManager loaded(configPath());
    ASSERT_TRUE(loaded.load());

    EXPECT_EQ(loaded.activeProfile(), QStringLiteral("glm5"));
    EXPECT_EQ(loaded.currentModel(), QStringLiteral("glm-5-turbo"));
    EXPECT_EQ(loaded.profileNames().size(), 2);
    EXPECT_TRUE(loaded.hasProfile(QStringLiteral("glm5")));
    EXPECT_TRUE(loaded.hasProfile(QStringLiteral("qwen3")));
}

TEST_F(ConfigManagerTest, RemoveProfileClearsActiveProfile) {
    ModelProfile p;
    p.name = QStringLiteral("glm5");
    p.model = QStringLiteral("glm-5-turbo");
    p.provider = QStringLiteral("openai_compat");
    ASSERT_TRUE(config()->addProfile(p));
    ASSERT_TRUE(config()->setActiveProfile(QStringLiteral("glm5")));
    EXPECT_EQ(config()->activeProfile(), QStringLiteral("glm5"));

    ASSERT_TRUE(config()->removeProfile(QStringLiteral("glm5")));
    EXPECT_TRUE(config()->activeProfile().isEmpty());
}

TEST_F(ConfigManagerTest, LoadOldConfigWithoutProfilesIsBackwardCompatible) {
    const QByteArray json = R"({
        "provider": "anthropic",
        "model": "claude-sonnet-4-20250514",
        "api_keys": {
            "anthropic": { "key": "sk-test" }
        }
    })";

    QFile file(configPath());
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(json);
    file.close();

    ASSERT_TRUE(config()->load());
    EXPECT_EQ(config()->currentModel(), QStringLiteral("claude-sonnet-4-20250514"));
    EXPECT_EQ(config()->provider(), QStringLiteral("anthropic"));
    EXPECT_TRUE(config()->activeProfile().isEmpty());
    EXPECT_TRUE(config()->profileNames().isEmpty());
}
