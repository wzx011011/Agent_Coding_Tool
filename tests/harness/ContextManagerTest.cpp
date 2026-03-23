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
