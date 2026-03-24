#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QJsonDocument>

#include <cstdlib>
#include <memory>

#include "services/ai_engine.h"
#include "services/config_manager.h"

using namespace act::services;

/// Integration tests that hit real LLM APIs.
/// Controlled by environment variables:
///   ACT_ANTHROPIC_API_KEY  — Anthropic provider test
///   ACT_ZHIPU_API_KEY      — GLM (OpenAI-compatible) provider test
/// Without the corresponding key, tests are skipped.
class LiveProviderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_tempDir.emplace();
        m_config = std::make_unique<ConfigManager>(m_tempDir->path());
    }

    ConfigManager *config() { return m_config.get(); }

    std::optional<QTemporaryDir> m_tempDir;
    std::unique_ptr<ConfigManager> m_config;
};

// --- Anthropic Live Tests ---

TEST_F(LiveProviderTest, AnthropicSimpleChat)
{
    auto key = std::getenv("ACT_ANTHROPIC_API_KEY");
    if (!key || key[0] == '\0')
        GTEST_SKIP() << "ACT_ANTHROPIC_API_KEY not set, skipping Anthropic test";

    config()->setProvider(QStringLiteral("anthropic"));
    config()->setApiKey(QStringLiteral("anthropic"), QString::fromUtf8(key));
    config()->setModel(QStringLiteral("claude-sonnet-4-20250514"));

    auto engine = new AIEngine(*(config()));

    QList<act::core::LLMMessage> messages;
    act::core::LLMMessage userMsg;
    userMsg.role = act::core::MessageRole::User;
    userMsg.content = QStringLiteral("Reply with exactly: HELLO_WORLD");
    messages.append(userMsg);

    act::core::LLMMessage response;
    bool completed = false;
    QString errorCode, errorMsg;

    engine->chat(
        messages,
        [&](act::core::LLMMessage msg) { response = msg; },
        [&] { completed = true; },
        [&](QString code, QString msg) {
            errorCode = code;
            errorMsg = msg;
        });

    ASSERT_TRUE(completed) << "Chat did not complete. Error: "
                           << errorCode.toStdString() << " - "
                           << errorMsg.toStdString();
    EXPECT_EQ(response.role, act::core::MessageRole::Assistant);
    EXPECT_FALSE(response.content.isEmpty());
}

// --- GLM Live Tests ---

TEST_F(LiveProviderTest, GLMSimpleChat)
{
    auto key = std::getenv("ACT_ZHIPU_API_KEY");
    if (!key || key[0] == '\0')
        GTEST_SKIP() << "ACT_ZHIPU_API_KEY not set, skipping GLM test";

    config()->setProvider(QStringLiteral("openai_compat"));
    config()->setApiKey(QStringLiteral("openai_compat"), QString::fromUtf8(key));
    config()->setModel(QStringLiteral("glm-4-flash"));
    config()->setBaseUrl(QString(ConfigManager::OPENAI_COMPAT_BASE_URL));

    auto engine = new AIEngine(*(config()));

    QList<act::core::LLMMessage> messages;
    act::core::LLMMessage userMsg;
    userMsg.role = act::core::MessageRole::User;
    userMsg.content = QStringLiteral("Reply with exactly: HELLO_GLM");
    messages.append(userMsg);

    act::core::LLMMessage response;
    bool completed = false;
    QString errorCode, errorMsg;

    engine->chat(
        messages,
        [&](act::core::LLMMessage msg) { response = msg; },
        [&] { completed = true; },
        [&](QString code, QString msg) {
            errorCode = code;
            errorMsg = msg;
        });

    ASSERT_TRUE(completed) << "Chat did not complete. Error: "
                           << errorCode.toStdString() << " - "
                           << errorMsg.toStdString();
    EXPECT_EQ(response.role, act::core::MessageRole::Assistant);
    EXPECT_FALSE(response.content.isEmpty());
}

// --- Error Handling ---

TEST_F(LiveProviderTest, AnthropicInvalidKeyReturnsAuthError)
{
    auto key = std::getenv("ACT_ANTHROPIC_API_KEY");
    if (!key || key[0] == '\0')
        GTEST_SKIP() << "ACT_ANTHROPIC_API_KEY not set, skipping auth error test";

    // Use an intentionally wrong key
    config()->setProvider(QStringLiteral("anthropic"));
    config()->setApiKey(QStringLiteral("anthropic"), QStringLiteral("sk-invalid-key-12345"));
    config()->setModel(QStringLiteral("claude-sonnet-4-20250514"));

    auto engine = new AIEngine(*(config()));

    QList<act::core::LLMMessage> messages;
    act::core::LLMMessage userMsg;
    userMsg.role = act::core::MessageRole::User;
    userMsg.content = QStringLiteral("Hello");
    messages.append(userMsg);

    bool completed = false;
    QString errorCode;

    engine->chat(
        messages,
        [](act::core::LLMMessage) {},
        [&] { completed = true; },
        [&](QString code, QString) { errorCode = code; });

    // Should get an error, not a successful completion
    // (may be AUTH_ERROR or HTTP_401 depending on network)
    EXPECT_FALSE(completed || errorCode.isEmpty());
}
