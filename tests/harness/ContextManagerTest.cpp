#include <gtest/gtest.h>

#include "harness/context_manager.h"

using namespace act::harness;
using namespace act::core;

// ============================================================
// ContextManager Tests
// ============================================================

class ContextManagerTest : public ::testing::Test
{
protected:
    ContextManager cm;
};

TEST_F(ContextManagerTest, DefaultMaxTokens)
{
    EXPECT_EQ(cm.maxContextTokens(), 200000);
}

TEST_F(ContextManagerTest, CustomMaxTokens)
{
    cm.setMaxContextTokens(100000);
    EXPECT_EQ(cm.maxContextTokens(), 100000);
}

TEST_F(ContextManagerTest, AutoCompactThreshold)
{
    cm.setMaxContextTokens(100000);
    // 80% of 100000 = 80000
    EXPECT_EQ(cm.autoCompactThreshold(), 80000);
    EXPECT_TRUE(cm.shouldAutoCompact(80000));
    EXPECT_FALSE(cm.shouldAutoCompact(79999));
}

TEST_F(ContextManagerTest, ManualCompactThreshold)
{
    cm.setMaxContextTokens(100000);
    // 90% of 100000 = 90000
    EXPECT_EQ(cm.manualCompactThreshold(), 90000);
    EXPECT_TRUE(cm.shouldSuggestCompact(90000));
    EXPECT_FALSE(cm.shouldSuggestCompact(89999));
}

TEST_F(ContextManagerTest, EstimateTokensEmpty)
{
    QList<LLMMessage> messages;
    EXPECT_EQ(cm.estimateTokens(messages), 0);
}

TEST_F(ContextManagerTest, EstimateTokensSimple)
{
    QList<LLMMessage> messages;
    LLMMessage msg;
    msg.role = MessageRole::User;
    msg.content = QStringLiteral("Hello, world!");  // 13 chars
    messages.append(msg);

    // (13 + 20 overhead) / 3.0 ≈ 11
    int tokens = cm.estimateTokens(messages);
    EXPECT_GT(tokens, 5);
    EXPECT_LT(tokens, 25);
}

TEST_F(ContextManagerTest, EstimateTokensMultipleMessages)
{
    QList<LLMMessage> messages;
    for (int i = 0; i < 10; ++i)
    {
        LLMMessage msg;
        msg.content = QStringLiteral("A");  // 1 char
        messages.append(msg);
    }

    // (10 * (1 + 20)) / 3.0 ≈ 70
    int tokens = cm.estimateTokens(messages);
    EXPECT_GT(tokens, 50);
    EXPECT_LT(tokens, 100);
}

TEST_F(ContextManagerTest, MicroCompactPreservesSystemMessage)
{
    QList<LLMMessage> messages;
    LLMMessage sys;
    sys.role = MessageRole::System;
    sys.content = QStringLiteral("You are a helpful assistant.");
    messages.append(sys);

    for (int i = 0; i < 5; ++i)
    {
        LLMMessage msg;
        msg.role = MessageRole::User;
        msg.content = QStringLiteral("Message %1").arg(i);
        messages.append(msg);
    }

    // Compact to 1 token (should keep system message + nothing else)
    auto compacted = cm.microCompact(messages, 1);
    ASSERT_EQ(compacted.size(), 1);
    EXPECT_EQ(compacted.first().role, MessageRole::System);
    EXPECT_FALSE(cm.lastCompactSummary().isEmpty());
}

TEST_F(ContextManagerTest, MicroCompactWithNoMessages)
{
    QList<LLMMessage> messages;
    auto compacted = cm.microCompact(messages, 1000);
    EXPECT_EQ(compacted.size(), 0);
}

TEST_F(ContextManagerTest, MicroCompactPreservesAllWhenBudget)
{
    QList<LLMMessage> messages;
    LLMMessage msg;
    msg.content = QStringLiteral("Short");
    messages.append(msg);

    auto compacted = cm.microCompact(messages, 100000);
    EXPECT_EQ(compacted.size(), messages.size());
}

TEST_F(ContextManagerTest, EstimateTokensWithToolCall)
{
    QList<LLMMessage> messages;
    LLMMessage msg;
    msg.role = MessageRole::Assistant;
    msg.content = QStringLiteral("Let me read that.");
    msg.toolCall.id = QStringLiteral("call_123");
    msg.toolCall.name = QStringLiteral("FileReadTool");
    messages.append(msg);

    int tokens = cm.estimateTokens(messages);
    EXPECT_GT(tokens, 10);
}

// --- N6: Three-layer compact tests ---

TEST_F(ContextManagerTest, AutoCompactDoesNothingWhenUnderThreshold)
{
    cm.setMaxContextTokens(200000);

    QList<LLMMessage> messages;
    LLMMessage msg;
    msg.content = QStringLiteral("Short message");
    messages.append(msg);

    auto result = cm.autoCompact(messages);
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(ContextManagerTest, AutoCompactTriggersWhenOverThreshold)
{
    cm.setMaxContextTokens(1000); // Low threshold for testing
    // autoCompactThreshold = 80% = 800

    QList<LLMMessage> messages;
    LLMMessage sys;
    sys.role = MessageRole::System;
    sys.content = QStringLiteral("System prompt");
    messages.append(sys);

    // Add enough messages to exceed threshold
    for (int i = 0; i < 50; ++i)
    {
        LLMMessage msg;
        msg.role = (i % 2 == 0) ? MessageRole::User : MessageRole::Assistant;
        msg.content = QStringLiteral("Message %1 with some content to accumulate tokens").arg(i);
        messages.append(msg);
    }

    auto result = cm.autoCompact(messages);
    EXPECT_FALSE(result.isEmpty());
    EXPECT_FALSE(cm.lastCompactSummary().isEmpty());
    // Result should be shorter than input
    EXPECT_LT(result.size(), messages.size());
}

TEST_F(ContextManagerTest, ManualCompactReplacesMiddleMessages)
{
    QList<LLMMessage> messages;
    LLMMessage sys;
    sys.role = MessageRole::System;
    sys.content = QStringLiteral("System");
    messages.append(sys);

    for (int i = 0; i < 10; ++i)
    {
        LLMMessage msg;
        msg.role = MessageRole::User;
        msg.content = QStringLiteral("Message %1").arg(i);
        messages.append(msg);
    }

    // Keep 2 most recent, compact the rest (excluding system)
    auto result = cm.manualCompact(messages, 2);

    // Expected: system + summary + 2 recent = 4
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0].role, MessageRole::System);
    EXPECT_TRUE(result[1].content.contains(QStringLiteral("compacted")));
    // The 2 most recent should be preserved
    EXPECT_EQ(result[2].content, QStringLiteral("Message 8"));
    EXPECT_EQ(result[3].content, QStringLiteral("Message 9"));
    EXPECT_FALSE(cm.lastCompactSummary().isEmpty());
}

TEST_F(ContextManagerTest, ManualCompactPreservesSystemMessage)
{
    QList<LLMMessage> messages;
    LLMMessage sys;
    sys.role = MessageRole::System;
    sys.content = QStringLiteral("Important system instructions");
    messages.append(sys);

    LLMMessage msg;
    msg.content = QStringLiteral("User message");
    messages.append(msg);

    auto result = cm.manualCompact(messages, 0);
    // System message always preserved, and with keepRecentCount=0 the
    // compactable range is messages.size()-1-0 = 1, which is the user msg
    // Actually with keepRecent=0 and only 1 non-system msg, compactable=1
    // Result: system + summary
    EXPECT_GE(result.size(), 1);
    EXPECT_EQ(result[0].role, MessageRole::System);
}

TEST_F(ContextManagerTest, ManualCompactReturnsAsIsWhenTooFewMessages)
{
    QList<LLMMessage> messages;
    LLMMessage msg;
    msg.content = QStringLiteral("Only one message");
    messages.append(msg);

    auto result = cm.manualCompact(messages);
    EXPECT_EQ(result.size(), 1);
}
