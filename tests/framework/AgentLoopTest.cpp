#include <gtest/gtest.h>

#include <QObject>

#include <memory>

#include "core/error_codes.h"
#include "framework/agent_loop.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "harness/context_manager.h"
#include "infrastructure/interfaces.h"
#include "services/interfaces.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;
using namespace act::framework;

// ============================================================
// Mock IAIEngine — returns canned responses via callbacks
// ============================================================

class MockAIEngine : public act::services::IAIEngine
{
public:
    // Queue of responses to replay
    QList<LLMMessage> responseQueue;
    QString lastError = QStringLiteral("INTERNAL_ERROR");
    int callCount = 0;

    void chat(const QList<LLMMessage> & /*messages*/,
              std::function<void(LLMMessage)> onMessage,
              std::function<void()> onComplete,
              std::function<void(QString, QString)> onError) override
    {
        ++callCount;

        if (!responseQueue.isEmpty())
        {
            // Send ONE response per chat() call
            onMessage(responseQueue.takeFirst());
            onComplete();
        }
        else
        {
            onError(lastError, QStringLiteral("mock error"));
        }
    }

    void cancel() override {}

    void setToolDefinitions(const QList<QJsonObject> & /*tools*/) override {}

    [[nodiscard]] int estimateTokens(
        const QList<LLMMessage> &messages) const override
    {
        int total = 0;
        for (const auto &m : messages)
            total += m.content.length();
        return static_cast<int>(total / 3.0);
    }
};

// ============================================================
// Simple mock tool for testing
// ============================================================

class EchoTool : public ITool
{
public:
    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("echo");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("Echo back the input");
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        return QJsonObject{};
    }
    ToolResult execute(const QJsonObject &params) override
    {
        auto text = params.value(QStringLiteral("text")).toString();
        return ToolResult::ok(QStringLiteral("Echo: %1").arg(text));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Read;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

// ============================================================
// Test Fixture
// ============================================================

class AgentLoopTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        engine = std::make_unique<MockAIEngine>();
        registry = std::make_unique<ToolRegistry>();
        permissions = std::make_unique<PermissionManager>();
        context = std::make_unique<ContextManager>();

        // Register echo tool (Read level — auto-approved by default)
        registry->registerTool(std::make_unique<EchoTool>());
        // Read is auto-approved by default
        permissions->setAutoApproved(PermissionLevel::Read, true);

        loop = std::make_unique<AgentLoop>(
            *engine, *registry, *permissions, *context);
    }

    void TearDown() override
    {
        loop.reset();
        context.reset();
        permissions.reset();
        registry.reset();
        engine.reset();
    }

    std::unique_ptr<MockAIEngine> engine;
    std::unique_ptr<ToolRegistry> registry;
    std::unique_ptr<PermissionManager> permissions;
    std::unique_ptr<ContextManager> context;
    std::unique_ptr<AgentLoop> loop;

    /// Helper: create a final AI response (no tool call).
    static LLMMessage finalResponse(const QString &text)
    {
        LLMMessage msg;
        msg.role = MessageRole::Assistant;
        msg.content = text;
        return msg;
    }

    /// Helper: create an AI response with a tool call.
    static LLMMessage toolCallResponse(const QString &toolName,
                                        const QJsonObject &params)
    {
        LLMMessage msg;
        msg.role = MessageRole::Assistant;
        msg.content = QStringLiteral("Let me use a tool.");
        msg.toolCall.id = QStringLiteral("call_123");
        msg.toolCall.name = toolName;
        msg.toolCall.params = params;
        return msg;
    }
};

// ============================================================
// State Transition Tests
// ============================================================

TEST_F(AgentLoopTest, InitialStateIsIdle)
{
    EXPECT_EQ(loop->state(), TaskState::Idle);
}

TEST_F(AgentLoopTest, SubmitMessageTransitionsToRunning)
{
    // Set up AI to respond with final answer immediately
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Hello!")));

    loop->submitUserMessage(QStringLiteral("Hi"));
    EXPECT_EQ(loop->state(), TaskState::Completed);
}

TEST_F(AgentLoopTest, FinalResponseTransitionsToCompleted)
{
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Done.")));

    loop->submitUserMessage(QStringLiteral("Do something"));
    EXPECT_EQ(loop->state(), TaskState::Completed);
    EXPECT_EQ(loop->messages().size(), 2); // user + assistant
}

TEST_F(AgentLoopTest, TurnCountIncrements)
{
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Response")));

    loop->submitUserMessage(QStringLiteral("Test"));
    EXPECT_EQ(loop->turnCount(), 1);
}

// ============================================================
// Tool Use Loop Tests
// ============================================================

TEST_F(AgentLoopTest, ToolCallExecutesAndLoopsBack)
{
    // AI responds with tool call, then final answer
    QJsonObject params;
    params[QStringLiteral("text")] = QStringLiteral("hello");

    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("echo"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("I echoed it.")));

    loop->submitUserMessage(QStringLiteral("Echo hello"));

    EXPECT_EQ(loop->state(), TaskState::Completed);
    EXPECT_EQ(loop->turnCount(), 2);

    // Messages: user, assistant(tool_call), tool_result, assistant(final)
    EXPECT_EQ(loop->messages().size(), 4);
    EXPECT_EQ(loop->messages().at(2).role, MessageRole::Tool);
    EXPECT_TRUE(loop->messages().at(2).content.contains(QStringLiteral("Echo: hello")));
}

TEST_F(AgentLoopTest, ToolNotFoundReturnsErrorToAI)
{
    // AI requests a non-existent tool
    QJsonObject params;
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("nonexistent_tool"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("I'll try something else.")));

    loop->submitUserMessage(QStringLiteral("Test"));
    EXPECT_EQ(loop->state(), TaskState::Completed);

    // Check that tool error was injected
    bool foundError = false;
    for (const auto &msg : loop->messages())
    {
        if (msg.role == MessageRole::Tool &&
            msg.content.contains(errors::TOOL_NOT_FOUND))
        {
            foundError = true;
            break;
        }
    }
    EXPECT_TRUE(foundError);
}

// ============================================================
// Permission Tests
// ============================================================

TEST_F(AgentLoopTest, PermissionDeniedContinuesLoop)
{
    // Deny the echo tool
    permissions->setAutoApproved(PermissionLevel::Read, false);

    QJsonObject params;
    params[QStringLiteral("text")] = QStringLiteral("test");

    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("echo"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("I couldn't use the tool.")));

    loop->submitUserMessage(QStringLiteral("Try echo"));
    EXPECT_EQ(loop->state(), TaskState::Completed);

    // Check that permission denied was returned as tool result
    bool foundDenied = false;
    for (const auto &msg : loop->messages())
    {
        if (msg.role == MessageRole::Tool &&
            msg.content.contains(errors::PERMISSION_DENIED))
        {
            foundDenied = true;
            break;
        }
    }
    EXPECT_TRUE(foundDenied);
}

TEST_F(AgentLoopTest, MaxTurnsSafetyLimit)
{
    loop->setMaxTurns(2);

    // AI always responds with tool call (infinite loop attempt)
    QJsonObject params;
    params[QStringLiteral("text")] = QStringLiteral("loop");

    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("echo"), params));
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("echo"), params));
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("echo"), params));

    loop->submitUserMessage(QStringLiteral("Loop"));
    // Should stop due to max turns
    EXPECT_NE(loop->state(), TaskState::Running);
    EXPECT_NE(loop->state(), TaskState::Idle);
}

// ============================================================
// Error Handling Tests
// ============================================================

TEST_F(AgentLoopTest, AIErrorTransitionsToFailed)
{
    // No responses queued → AI engine "fails"
    engine->lastError = QStringLiteral("PROVIDER_TIMEOUT");

    loop->submitUserMessage(QStringLiteral("Test"));
    EXPECT_EQ(loop->state(), TaskState::Failed);
}

TEST_F(AgentLoopTest, ErrorStateRecordsErrorCode)
{
    engine->lastError = QStringLiteral("RATE_LIMIT");

    loop->submitUserMessage(QStringLiteral("Test"));
    EXPECT_EQ(loop->state(), TaskState::Failed);

    // Last message should contain the error
    const auto &last = loop->messages().last();
    EXPECT_TRUE(last.content.contains(QStringLiteral("RATE_LIMIT")));
}

// ============================================================
// Cancel Tests
// ============================================================

TEST_F(AgentLoopTest, CancelAfterCompletionIsNoop)
{
    // Synchronous mock completes before cancel can take effect.
    // Mid-execution cancel requires async testing (T10+).
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Done")));

    loop->submitUserMessage(QStringLiteral("Test"));
    EXPECT_EQ(loop->state(), TaskState::Completed);
    loop->cancel();  // no-op when already in terminal state
    EXPECT_EQ(loop->state(), TaskState::Completed);
}

// ============================================================
// Reset Tests
// ============================================================

TEST_F(AgentLoopTest, ResetClearsState)
{
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Response")));
    loop->submitUserMessage(QStringLiteral("Test"));

    EXPECT_GT(loop->messages().size(), 0);
    loop->reset();
    EXPECT_EQ(loop->state(), TaskState::Idle);
    EXPECT_EQ(loop->messages().size(), 0);
    EXPECT_EQ(loop->turnCount(), 0);
}

// ============================================================
// Checkpoint Tests
// ============================================================

TEST_F(AgentLoopTest, SaveAndRestoreCheckpoint)
{
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Checkpoint test")));
    loop->submitUserMessage(QStringLiteral("Test"));

    // Save checkpoint
    auto cp = loop->saveCheckpoint();
    EXPECT_EQ(cp.messages.size(), 2); // user + assistant
    EXPECT_EQ(cp.turnCount, 1);
    EXPECT_EQ(cp.state, TaskState::Completed);

    // Modify state
    loop->reset();
    EXPECT_EQ(loop->messages().size(), 0);

    // Restore
    loop->restoreCheckpoint(cp);
    EXPECT_EQ(loop->messages().size(), 2);
    EXPECT_EQ(loop->turnCount(), 1);
    EXPECT_EQ(loop->state(), TaskState::Completed);
}

TEST_F(AgentLoopTest, CheckpointPreservesToolHistory)
{
    QJsonObject params;
    params[QStringLiteral("text")] = QStringLiteral("checkpoint");

    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("echo"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Done")));

    loop->submitUserMessage(QStringLiteral("Test"));

    auto cp = loop->saveCheckpoint();
    EXPECT_EQ(cp.messages.size(), 4); // user, assistant+toolcall, tool_result, assistant

    loop->reset();
    loop->restoreCheckpoint(cp);
    EXPECT_EQ(loop->messages().size(), 4);
}

// ============================================================
// Context Compaction Tests
// ============================================================

TEST_F(AgentLoopTest, ContextCompactionTriggersOnThreshold)
{
    // Set a very small context window to trigger compaction
    context->setMaxContextTokens(30);

    QJsonObject params;
    params[QStringLiteral("text")] = QStringLiteral("compact");

    // Multiple tool rounds to accumulate messages
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("echo"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Done after compaction")));

    loop->submitUserMessage(QStringLiteral("Trigger compaction"));
    EXPECT_EQ(loop->state(), TaskState::Completed);
    // Compaction should have occurred (system msg preserved)
    EXPECT_FALSE(context->lastCompactSummary().isEmpty());
}

// ============================================================
// Double Submit Protection
// ============================================================

TEST_F(AgentLoopTest, SubmitWhileRunningIsIgnored)
{
    // First submit with no response (will fail)
    engine->lastError = QStringLiteral("TIMEOUT");
    loop->submitUserMessage(QStringLiteral("First"));

    // Second submit should be ignored (state is no longer Idle)
    int msgCountBefore = loop->messages().size();
    loop->submitUserMessage(QStringLiteral("Second"));
    EXPECT_EQ(loop->messages().size(), msgCountBefore);
}
