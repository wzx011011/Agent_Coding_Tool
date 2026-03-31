#include <gtest/gtest.h>

#include <QObject>

#include <memory>

#include "core/error_codes.h"
#include "framework/agent_loop.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "harness/context_manager.h"
#include "harness/tools/enter_plan_mode_tool.h"
#include "harness/tools/exit_plan_mode_tool.h"
#include "infrastructure/interfaces.h"
#include "services/interfaces.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;
using namespace act::framework;

// ============================================================
// Mock IAIEngine
// ============================================================

class MockAIEngine : public act::services::IAIEngine
{
public:
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
// Mock tools with different permission levels
// ============================================================

class ReadTool : public ITool
{
public:
    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("read_tool");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("A read-only tool");
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        return QJsonObject{};
    }
    ToolResult execute(const QJsonObject & /*params*/) override
    {
        return ToolResult::ok(QStringLiteral("read result"));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Read;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

class WriteTool : public ITool
{
public:
    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("write_tool");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("A write tool");
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        return QJsonObject{};
    }
    ToolResult execute(const QJsonObject & /*params*/) override
    {
        return ToolResult::ok(QStringLiteral("write result"));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Write;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

class ExecTool : public ITool
{
public:
    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("exec_tool");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("An exec tool");
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        return QJsonObject{};
    }
    ToolResult execute(const QJsonObject & /*params*/) override
    {
        return ToolResult::ok(QStringLiteral("exec result"));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Exec;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

class NetworkTool : public ITool
{
public:
    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("network_tool");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("A network tool");
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        return QJsonObject{};
    }
    ToolResult execute(const QJsonObject & /*params*/) override
    {
        return ToolResult::ok(QStringLiteral("network result"));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Network;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

class DestructiveTool : public ITool
{
public:
    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("destructive_tool");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("A destructive tool");
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        return QJsonObject{};
    }
    ToolResult execute(const QJsonObject & /*params*/) override
    {
        return ToolResult::ok(QStringLiteral("destructive result"));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Destructive;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

// ============================================================
// Test Fixture
// ============================================================

class PlanModeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        engine = std::make_unique<MockAIEngine>();
        registry = std::make_unique<ToolRegistry>();
        permissions = std::make_unique<PermissionManager>();
        context = std::make_unique<ContextManager>();

        registry->registerTool(std::make_unique<ReadTool>());
        registry->registerTool(std::make_unique<WriteTool>());
        registry->registerTool(std::make_unique<ExecTool>());
        registry->registerTool(std::make_unique<NetworkTool>());
        registry->registerTool(std::make_unique<DestructiveTool>());

        // Auto-approve all levels for testing
        permissions->setAutoApproved(PermissionLevel::Read, true);
        permissions->setAutoApproved(PermissionLevel::Write, true);
        permissions->setAutoApproved(PermissionLevel::Exec, true);
        permissions->setAutoApproved(PermissionLevel::Network, true);
        permissions->setAutoApproved(PermissionLevel::Destructive, true);

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

    static LLMMessage finalResponse(const QString &text)
    {
        LLMMessage msg;
        msg.role = MessageRole::Assistant;
        msg.content = text;
        return msg;
    }

    static LLMMessage toolCallResponse(const QString &toolName,
                                        const QJsonObject &params)
    {
        LLMMessage msg;
        msg.role = MessageRole::Assistant;
        msg.content = QStringLiteral("Using tool.");
        msg.toolCall.id = QStringLiteral("call_123");
        msg.toolCall.name = toolName;
        msg.toolCall.params = params;
        return msg;
    }

    std::unique_ptr<MockAIEngine> engine;
    std::unique_ptr<ToolRegistry> registry;
    std::unique_ptr<PermissionManager> permissions;
    std::unique_ptr<ContextManager> context;
    std::unique_ptr<AgentLoop> loop;
};

// ============================================================
// Plan Mode State Transition Tests
// ============================================================

TEST_F(PlanModeTest, InitialStateIsNotPlanMode)
{
    EXPECT_FALSE(loop->isPlanMode());
    EXPECT_EQ(loop->state(), TaskState::Idle);
}

TEST_F(PlanModeTest, EnterPlanModeTransitionsToPlanning)
{
    loop->enterPlanMode();
    EXPECT_TRUE(loop->isPlanMode());
    EXPECT_EQ(loop->state(), TaskState::Planning);
}

TEST_F(PlanModeTest, ExitPlanModeTransitionsBackToIdle)
{
    loop->enterPlanMode();
    loop->exitPlanMode();
    EXPECT_FALSE(loop->isPlanMode());
    EXPECT_EQ(loop->state(), TaskState::Idle);
}

TEST_F(PlanModeTest, EnterPlanModeIdempotent)
{
    loop->enterPlanMode();
    loop->enterPlanMode();
    EXPECT_TRUE(loop->isPlanMode());
    EXPECT_EQ(loop->state(), TaskState::Planning);
}

TEST_F(PlanModeTest, ExitPlanModeIdempotent)
{
    loop->exitPlanMode();
    EXPECT_FALSE(loop->isPlanMode());
    EXPECT_EQ(loop->state(), TaskState::Idle);
}

TEST_F(PlanModeTest, EnterPlanModeFromCompletedState)
{
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Done")));
    loop->submitUserMessage(QStringLiteral("Test"));
    ASSERT_EQ(loop->state(), TaskState::Completed);

    loop->enterPlanMode();
    EXPECT_TRUE(loop->isPlanMode());
    EXPECT_EQ(loop->state(), TaskState::Planning);
}

// ============================================================
// Plan Mode Tool Filtering Tests
// ============================================================

TEST_F(PlanModeTest, ReadToolAllowedInPlanMode)
{
    loop->enterPlanMode();

    QJsonObject params;
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("read_tool"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Read done.")));

    loop->submitUserMessage(QStringLiteral("Read something"));
    EXPECT_EQ(loop->state(), TaskState::Completed);

    // Verify the read tool was executed (not blocked)
    bool readExecuted = false;
    for (const auto &msg : loop->messages())
    {
        if (msg.role == MessageRole::Tool &&
            msg.content.contains(QStringLiteral("read result")))
        {
            readExecuted = true;
            break;
        }
    }
    EXPECT_TRUE(readExecuted);
}

TEST_F(PlanModeTest, NetworkToolAllowedInPlanMode)
{
    loop->enterPlanMode();

    QJsonObject params;
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("network_tool"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Network done.")));

    loop->submitUserMessage(QStringLiteral("Fetch something"));
    EXPECT_EQ(loop->state(), TaskState::Completed);

    bool networkExecuted = false;
    for (const auto &msg : loop->messages())
    {
        if (msg.role == MessageRole::Tool &&
            msg.content.contains(QStringLiteral("network result")))
        {
            networkExecuted = true;
            break;
        }
    }
    EXPECT_TRUE(networkExecuted);
}

TEST_F(PlanModeTest, WriteToolBlockedInPlanMode)
{
    loop->enterPlanMode();

    QJsonObject params;
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("write_tool"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Write was blocked.")));

    loop->submitUserMessage(QStringLiteral("Try write"));
    EXPECT_EQ(loop->state(), TaskState::Completed);

    // Verify the write tool was blocked
    bool writeBlocked = false;
    for (const auto &msg : loop->messages())
    {
        if (msg.role == MessageRole::Tool &&
            msg.content.contains(errors::PERMISSION_DENIED))
        {
            writeBlocked = true;
            break;
        }
    }
    EXPECT_TRUE(writeBlocked);
}

TEST_F(PlanModeTest, ExecToolBlockedInPlanMode)
{
    loop->enterPlanMode();

    QJsonObject params;
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("exec_tool"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Exec was blocked.")));

    loop->submitUserMessage(QStringLiteral("Try exec"));
    EXPECT_EQ(loop->state(), TaskState::Completed);

    bool execBlocked = false;
    for (const auto &msg : loop->messages())
    {
        if (msg.role == MessageRole::Tool &&
            msg.content.contains(errors::PERMISSION_DENIED))
        {
            execBlocked = true;
            break;
        }
    }
    EXPECT_TRUE(execBlocked);
}

TEST_F(PlanModeTest, DestructiveToolBlockedInPlanMode)
{
    loop->enterPlanMode();

    QJsonObject params;
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("destructive_tool"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Destructive was blocked.")));

    loop->submitUserMessage(QStringLiteral("Try destructive"));
    EXPECT_EQ(loop->state(), TaskState::Completed);

    bool destructiveBlocked = false;
    for (const auto &msg : loop->messages())
    {
        if (msg.role == MessageRole::Tool &&
            msg.content.contains(errors::PERMISSION_DENIED))
        {
            destructiveBlocked = true;
            break;
        }
    }
    EXPECT_TRUE(destructiveBlocked);
}

// ============================================================
// Plan Mode Transition During Execution
// ============================================================

TEST_F(PlanModeTest, WriteToolAllowedAfterExitingPlanMode)
{
    loop->enterPlanMode();
    loop->exitPlanMode();

    QJsonObject params;
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("write_tool"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Write done.")));

    loop->submitUserMessage(QStringLiteral("Write something"));
    EXPECT_EQ(loop->state(), TaskState::Completed);

    bool writeExecuted = false;
    for (const auto &msg : loop->messages())
    {
        if (msg.role == MessageRole::Tool &&
            msg.content.contains(QStringLiteral("write result")))
        {
            writeExecuted = true;
            break;
        }
    }
    EXPECT_TRUE(writeExecuted);
}

// ============================================================
// Plan Mode Preserves Conversation History
// ============================================================

TEST_F(PlanModeTest, PlanModePreservesConversation)
{
    engine->responseQueue.append(
        finalResponse(QStringLiteral("First response.")));
    loop->submitUserMessage(QStringLiteral("First message."));
    ASSERT_EQ(loop->messages().size(), 2);

    // Enter plan mode
    loop->enterPlanMode();

    // Submit another message in plan mode
    QJsonObject params;
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("read_tool"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Read in plan mode.")));
    loop->submitUserMessage(QStringLiteral("Read something."));

    // Should have all messages preserved
    EXPECT_GT(loop->messages().size(), 2);
}

// ============================================================
// EnterPlanModeTool Tests
// ============================================================

TEST_F(PlanModeTest, EnterPlanModeToolExecute)
{
    auto tool = EnterPlanModeTool(*loop);

    EXPECT_EQ(tool.name(), QStringLiteral("enter_plan_mode"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);

    auto result = tool.execute(QJsonObject{});
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(loop->isPlanMode());
}

TEST_F(PlanModeTest, EnterPlanModeToolAlreadyInPlanMode)
{
    loop->enterPlanMode();
    auto tool = EnterPlanModeTool(*loop);

    auto result = tool.execute(QJsonObject{});
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.contains(QStringLiteral("Already")));
}

// ============================================================
// ExitPlanModeTool Tests
// ============================================================

TEST_F(PlanModeTest, ExitPlanModeToolExecute)
{
    loop->enterPlanMode();
    auto tool = ExitPlanModeTool(*loop);

    EXPECT_EQ(tool.name(), QStringLiteral("exit_plan_mode"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);

    auto result = tool.execute(QJsonObject{});
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(loop->isPlanMode());
}

TEST_F(PlanModeTest, ExitPlanModeToolNotInPlanMode)
{
    auto tool = ExitPlanModeTool(*loop);

    auto result = tool.execute(QJsonObject{});
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.contains(QStringLiteral("Not in")));
}

// ============================================================
// Plan Mode Error Message Content
// ============================================================

TEST_F(PlanModeTest, PlanModeBlockMessageMentionsPlanMode)
{
    loop->enterPlanMode();

    QJsonObject params;
    engine->responseQueue.append(
        toolCallResponse(QStringLiteral("write_tool"), params));
    engine->responseQueue.append(
        finalResponse(QStringLiteral("Blocked.")));

    loop->submitUserMessage(QStringLiteral("Try write"));

    bool foundPlanModeMsg = false;
    for (const auto &msg : loop->messages())
    {
        if (msg.role == MessageRole::Tool &&
            msg.content.contains(QStringLiteral("Plan Mode")))
        {
            foundPlanModeMsg = true;
            break;
        }
    }
    EXPECT_TRUE(foundPlanModeMsg);
}
