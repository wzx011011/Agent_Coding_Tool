#include <gtest/gtest.h>

#include <QStringList>
#include <memory>

#include "core/error_codes.h"
#include "framework/cli_repl.h"
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
// Mock AIEngine (same pattern as AgentLoopTest)
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
// Echo tool (Read level, auto-approved)
// ============================================================

class EchoTool : public ITool
{
public:
    [[nodiscard]] QString name() const override { return QStringLiteral("echo"); }
    [[nodiscard]] QString description() const override { return QStringLiteral("Echo"); }
    [[nodiscard]] QJsonObject schema() const override { return QJsonObject{}; }
    ToolResult execute(const QJsonObject &params) override
    {
        return ToolResult::ok(QStringLiteral("Echo: %1")
            .arg(params.value(QStringLiteral("text")).toString()));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override { return PermissionLevel::Read; }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

// ============================================================
// Test Fixture
// ============================================================

class CliReplTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        engine = std::make_unique<MockAIEngine>();
        registry = std::make_unique<ToolRegistry>();
        permissions = std::make_unique<PermissionManager>();
        context = std::make_unique<ContextManager>();
        registry->registerTool(std::make_unique<EchoTool>());
        permissions->setAutoApproved(PermissionLevel::Read, true);

        repl = std::make_unique<CliRepl>(
            *engine, *registry, *permissions, *context);

        capturedLines.clear();
        QObject::connect(repl.get(), &CliRepl::outputLine,
                         [this](const QString &line) { capturedLines.append(line); });
        QObject::connect(repl.get(), &CliRepl::jsonEvent,
                         [this](const QString &line) { capturedLines.append(line); });
    }

    void TearDown() override
    {
        repl.reset();
        context.reset();
        permissions.reset();
        registry.reset();
        engine.reset();
    }

    std::unique_ptr<MockAIEngine> engine;
    std::unique_ptr<ToolRegistry> registry;
    std::unique_ptr<PermissionManager> permissions;
    std::unique_ptr<ContextManager> context;
    std::unique_ptr<CliRepl> repl;
    QStringList capturedLines;

    static LLMMessage finalResponse(const QString &text)
    {
        LLMMessage msg;
        msg.role = MessageRole::Assistant;
        msg.content = text;
        return msg;
    }
};

// ============================================================
// Human Mode Tests
// ============================================================

TEST_F(CliReplTest, HumanModeOutputsPrompt)
{
    repl->setOutputMode(CliRepl::OutputMode::Human);
    engine->responseQueue.append(finalResponse(QStringLiteral("Hello!")));
    repl->processInput(QStringLiteral("Hi"));
    EXPECT_TRUE(capturedLines.contains(QStringLiteral("> Hi")));
}

TEST_F(CliReplTest, HumanModeOutputsAssistantResponse)
{
    repl->setOutputMode(CliRepl::OutputMode::Human);
    engine->responseQueue.append(finalResponse(QStringLiteral("Hello!")));
    repl->processInput(QStringLiteral("Hi"));
    // In Human mode, assistant text is streamed via AIEngine::streamTokenReceived
    // (connected in main.cpp), not via outputLine. CliRepl emits an empty string
    // after the agent loop completes to ensure a trailing newline after streaming.
    EXPECT_TRUE(capturedLines.contains(QStringLiteral("> Hi")));
    EXPECT_TRUE(capturedLines.contains(QString()));
}

TEST_F(CliReplTest, EmptyInputReturnsIdle)
{
    auto state = repl->processInput(QStringLiteral(""));
    EXPECT_EQ(state, TaskState::Idle);
}

TEST_F(CliReplTest, SlashExitReturnsIdle)
{
    auto state = repl->processInput(QStringLiteral("/exit"));
    EXPECT_EQ(state, TaskState::Idle);
}

TEST_F(CliReplTest, SlashQuitReturnsIdle)
{
    auto state = repl->processInput(QStringLiteral("/quit"));
    EXPECT_EQ(state, TaskState::Idle);
}

TEST_F(CliReplTest, SlashResetReturnsIdle)
{
    auto state = repl->processInput(QStringLiteral("/reset"));
    EXPECT_EQ(state, TaskState::Idle);
    EXPECT_TRUE(capturedLines.contains(QStringLiteral("[System] Conversation reset.")));
}

TEST_F(CliReplTest, SlashStatusReturnsIdle)
{
    auto state = repl->processInput(QStringLiteral("/status"));
    EXPECT_EQ(state, TaskState::Idle);
    EXPECT_TRUE(capturedLines.at(0).contains(QStringLiteral("[System] State:")));
}

TEST_F(CliReplTest, WhitespaceInputReturnsIdle)
{
    auto state = repl->processInput(QStringLiteral("   "));
    EXPECT_EQ(state, TaskState::Idle);
}

// ============================================================
// JSON Mode Tests
// ============================================================

TEST_F(CliReplTest, JsonModeEmitsUserMessage)
{
    repl->setOutputMode(CliRepl::OutputMode::Json);
    engine->responseQueue.append(finalResponse(QStringLiteral("OK")));
    repl->processInput(QStringLiteral("test input"));

    bool foundUser = false;
    for (const auto &line : capturedLines)
    {
        if (line.contains(QStringLiteral("\"type\"")) &&
            line.contains(QStringLiteral("\"user\"")))
        {
            foundUser = true;
            break;
        }
    }
    EXPECT_TRUE(foundUser);
}

TEST_F(CliReplTest, JsonModeEmitsAssistantMessage)
{
    repl->setOutputMode(CliRepl::OutputMode::Json);
    engine->responseQueue.append(finalResponse(QStringLiteral("OK")));
    repl->processInput(QStringLiteral("test"));

    bool foundAssistant = false;
    for (const auto &line : capturedLines)
    {
        if (line.contains(QStringLiteral("\"type\"")) &&
            line.contains(QStringLiteral("\"assistant\"")))
        {
            foundAssistant = true;
            break;
        }
    }
    EXPECT_TRUE(foundAssistant);
}

TEST_F(CliReplTest, JsonLinesAreValid)
{
    repl->setOutputMode(CliRepl::OutputMode::Json);
    engine->responseQueue.append(finalResponse(QStringLiteral("OK")));
    repl->processInput(QStringLiteral("test"));

    for (const auto &line : capturedLines)
    {
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        EXPECT_FALSE(doc.isNull()) << "Invalid JSON: " << line.toStdString();
    }
}

// ============================================================
// Error State Tests
// ============================================================

TEST_F(CliReplTest, AIErrorReportsState)
{
    repl->setOutputMode(CliRepl::OutputMode::Human);
    engine->lastError = QStringLiteral("PROVIDER_TIMEOUT");
    repl->processInput(QStringLiteral("test"));

    EXPECT_TRUE(capturedLines.contains(QStringLiteral("[System] Agent failed.")));
}

// ============================================================
// Batch Mode Tests
// ============================================================

TEST_F(CliReplTest, BatchModeProcessesMultipleInputs)
{
    repl->setOutputMode(CliRepl::OutputMode::Human);
    engine->responseQueue.append(finalResponse(QStringLiteral("One")));
    engine->responseQueue.append(finalResponse(QStringLiteral("Two")));
    engine->responseQueue.append(finalResponse(QStringLiteral("Three")));

    repl->processBatch(
        {QStringLiteral("first"), QStringLiteral("second"), QStringLiteral("third")});

    EXPECT_GE(capturedLines.size(), 3);
}

TEST_F(CliReplTest, BatchModeStopsOnExit)
{
    repl->setOutputMode(CliRepl::OutputMode::Human);
    engine->responseQueue.append(finalResponse(QStringLiteral("Only this")));

    repl->processBatch({QStringLiteral("hi"), QStringLiteral("/exit"), QStringLiteral("never")});

    // Only one prompt should be processed (plus its response)
    bool foundNever = false;
    for (const auto &line : capturedLines)
    {
        if (line.contains(QStringLiteral("never")))
            foundNever = true;
    }
    EXPECT_FALSE(foundNever);
}

// ============================================================
// Command Registry Tests
// ============================================================

TEST_F(CliReplTest, HelpCommandListsCommands)
{
    repl->setOutputMode(CliRepl::OutputMode::Human);
    auto state = repl->processInput(QStringLiteral("/help"));
    EXPECT_EQ(state, TaskState::Idle);

    // Should show header and at least the built-in commands
    bool foundHeader = false;
    bool foundExit = false;
    bool foundReset = false;
    for (const auto &line : capturedLines)
    {
        if (line.contains(QStringLiteral("Available commands")))
            foundHeader = true;
        if (line.contains(QStringLiteral("/exit")))
            foundExit = true;
        if (line.contains(QStringLiteral("/reset")))
            foundReset = true;
    }
    EXPECT_TRUE(foundHeader);
    EXPECT_TRUE(foundExit);
    EXPECT_TRUE(foundReset);
}

TEST_F(CliReplTest, UnknownCommandShowsError)
{
    repl->setOutputMode(CliRepl::OutputMode::Human);
    auto state = repl->processInput(QStringLiteral("/unknowncommand"));
    EXPECT_EQ(state, TaskState::Idle);

    bool foundError = false;
    for (const auto &line : capturedLines)
    {
        if (line.contains(QStringLiteral("Unknown command")))
            foundError = true;
    }
    EXPECT_TRUE(foundError);
}

TEST_F(CliReplTest, ExitRequestedFlag)
{
    EXPECT_FALSE(repl->isExitRequested());
    repl->processInput(QStringLiteral("/exit"));
    EXPECT_TRUE(repl->isExitRequested());
}

TEST_F(CliReplTest, ExitRequestedFlagReset)
{
    repl->processInput(QStringLiteral("/exit"));
    EXPECT_TRUE(repl->isExitRequested());
    repl->clearExitRequested();
    EXPECT_FALSE(repl->isExitRequested());
}

TEST_F(CliReplTest, CommandRegistryRegisterCommand)
{
    auto &registry = repl->commandRegistry();
    int initialCount = registry.commandCount();

    bool registered = registry.registerCommand(
        QStringLiteral("testcmd"),
        QStringLiteral("Test command"),
        [](const QStringList &) -> bool { return true; });

    EXPECT_TRUE(registered);
    EXPECT_EQ(registry.commandCount(), initialCount + 1);
    EXPECT_TRUE(registry.hasCommand(QStringLiteral("testcmd")));
}

TEST_F(CliReplTest, CommandRegistryDuplicateFails)
{
    auto &registry = repl->commandRegistry();

    bool first = registry.registerCommand(
        QStringLiteral("dupcmd"),
        QStringLiteral("First"),
        [](const QStringList &) -> bool { return true; });
    EXPECT_TRUE(first);

    bool second = registry.registerCommand(
        QStringLiteral("dupcmd"),
        QStringLiteral("Second"),
        [](const QStringList &) -> bool { return true; });
    EXPECT_FALSE(second);
}

TEST_F(CliReplTest, CommandRegistryUnregisterCommand)
{
    auto &registry = repl->commandRegistry();

    registry.registerCommand(
        QStringLiteral("tempcmd"),
        QStringLiteral("Temp"),
        [](const QStringList &) -> bool { return true; });
    EXPECT_TRUE(registry.hasCommand(QStringLiteral("tempcmd")));

    bool removed = registry.unregisterCommand(QStringLiteral("tempcmd"));
    EXPECT_TRUE(removed);
    EXPECT_FALSE(registry.hasCommand(QStringLiteral("tempcmd")));
}

TEST_F(CliReplTest, CommandRegistryExecute)
{
    auto &registry = repl->commandRegistry();
    bool executed = false;

    registry.registerCommand(
        QStringLiteral("execcmd"),
        QStringLiteral("Exec test"),
        [&executed](const QStringList &args) -> bool {
            executed = true;
            return args.isEmpty() || args.contains(QStringLiteral("ok"));
        });

    EXPECT_TRUE(registry.execute(QStringLiteral("execcmd"), {}));
    EXPECT_TRUE(executed);

    executed = false;
    EXPECT_TRUE(registry.execute(QStringLiteral("execcmd"), {QStringLiteral("ok")}));
    EXPECT_TRUE(executed);
}

TEST_F(CliReplTest, CommandRegistryExecuteNonexistent)
{
    auto &registry = repl->commandRegistry();
    EXPECT_FALSE(registry.execute(QStringLiteral("nonexistent"), {}));
}

TEST_F(CliReplTest, CommandRegistryListCommands)
{
    auto &registry = repl->commandRegistry();
    auto commands = registry.listCommands();

    // Should have at least the built-in commands
    EXPECT_GE(commands.size(), 4);

    // Verify each command has required fields
    for (const auto &cmd : commands)
    {
        EXPECT_FALSE(cmd.name.isEmpty());
        EXPECT_FALSE(cmd.description.isEmpty());
        EXPECT_TRUE(cmd.handler);
    }
}

TEST_F(CliReplTest, CustomCommandViaProcessInput)
{
    auto &registry = repl->commandRegistry();
    bool customExecuted = false;

    registry.registerCommand(
        QStringLiteral("custom"),
        QStringLiteral("Custom test command"),
        [&customExecuted](const QStringList &) -> bool {
            customExecuted = true;
            return true;
        });

    auto state = repl->processInput(QStringLiteral("/custom"));
    EXPECT_EQ(state, TaskState::Idle);
    EXPECT_TRUE(customExecuted);
}
