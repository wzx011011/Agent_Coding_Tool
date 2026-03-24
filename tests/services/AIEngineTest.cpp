#include <gtest/gtest.h>

#include <QTemporaryDir>

#include "services/ai_engine.h"
#include "services/config_manager.h"

using namespace act::services;

class AIEngineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_tempDir.emplace();
        m_config = std::make_unique<ConfigManager>(m_tempDir->path());
        m_engine = std::make_unique<AIEngine>(*m_config);
    }

    void resetEngine()
    {
        m_engine = std::make_unique<AIEngine>(*m_config);
    }

    AIEngine *engine() { return m_engine.get(); }
    ConfigManager *config() { return m_config.get(); }

    std::optional<QTemporaryDir> m_tempDir;
    std::unique_ptr<ConfigManager> m_config;
    std::unique_ptr<AIEngine> m_engine;
};

TEST_F(AIEngineTest, EstimateTokensWithEmptyMessages)
{
    QList<act::core::LLMMessage> messages;
    EXPECT_EQ(engine()->estimateTokens(messages), 0);
}

TEST_F(AIEngineTest, EstimateTokensSimple)
{
    QList<act::core::LLMMessage> messages;
    act::core::LLMMessage msg;
    msg.content = QStringLiteral("Hello, world!");  // 13 chars
    messages.append(msg);

    // Expected: (13 + 0 + 0 + 50) / 3.5 ≈ 18
    int tokens = engine()->estimateTokens(messages);
    EXPECT_GT(tokens, 10);
    EXPECT_LT(tokens, 30);
}

TEST_F(AIEngineTest, EstimateTokensMultipleMessages)
{
    QList<act::core::LLMMessage> messages;
    for (int i = 0; i < 5; ++i)
    {
        act::core::LLMMessage msg;
        msg.content = QStringLiteral("A");
        messages.append(msg);
    }

    // (5 * 1 + 5 * 50) / 3.5 ≈ 72
    int tokens = engine()->estimateTokens(messages);
    EXPECT_GT(tokens, 50);
    EXPECT_LT(tokens, 100);
}

TEST_F(AIEngineTest, EstimateTokensWithToolCall)
{
    QList<act::core::LLMMessage> messages;
    act::core::LLMMessage msg;
    msg.role = act::core::MessageRole::Assistant;
    msg.content = QStringLiteral("Let me read that file.");
    msg.toolCall.id = QStringLiteral("call_123");
    msg.toolCall.name = QStringLiteral("FileReadTool");
    messages.append(msg);

    // (17 + 0 + 9 + 50) / 3.5 ≈ 22
    int tokens = engine()->estimateTokens(messages);
    EXPECT_GT(tokens, 15);
    EXPECT_LT(tokens, 35);
}

TEST_F(AIEngineTest, ChatWithoutProviderReturnsError)
{
    QList<act::core::LLMMessage> messages;
    act::core::LLMMessage msg;
    msg.content = QStringLiteral("Hello");
    messages.append(msg);

    QString errorCode;
    QString errorMessage;
    bool completed = false;

    engine()->chat(
        messages,
        [](act::core::LLMMessage) {},
        [&] { completed = true; },
        [&](QString code, QString msg) {
            errorCode = code;
            errorMessage = msg;
        });

    EXPECT_FALSE(errorCode.isEmpty());
    EXPECT_FALSE(errorMessage.isEmpty());
    EXPECT_FALSE(completed);
}

TEST_F(AIEngineTest, ChatWithConfiguredProviderReturnsNetworkError)
{
    config()->setApiKey(QStringLiteral("anthropic"), QStringLiteral("sk-test"));
    // Need to re-create engine to pick up the new key
    resetEngine();

    QList<act::core::LLMMessage> messages;
    act::core::LLMMessage msg;
    msg.content = QStringLiteral("Hello");
    messages.append(msg);

    bool gotResponse = false;
    bool completed = false;
    bool gotError = false;
    QString errorCode;

    // The provider may throw without SSL support — catch it
    try
    {
        engine()->chat(
            messages,
            [&](act::core::LLMMessage) { gotResponse = true; },
            [&] { completed = true; },
            [&](QString code, QString) {
                errorCode = code;
                gotError = true;
            });
    }
    catch (const std::exception &)
    {
        gotError = true;
    }

    // Without real HTTPS support, the request will fail
    EXPECT_FALSE(gotResponse);
    EXPECT_TRUE(gotError);
}
