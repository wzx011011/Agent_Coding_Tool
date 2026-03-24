#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QStringList>
#include <memory>

#include "core/error_codes.h"
#include "core/types.h"
#include "framework/agent_loop.h"
#include "framework/cli_repl.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "infrastructure/interfaces.h"
#include "services/interfaces.h"

using namespace act::core;
using namespace act::harness;
using namespace act::framework;

// ============================================================
// Mock AIEngine that simulates a full tool-use cycle
// ============================================================

class ScenarioEngine : public act::services::IAIEngine
{
public:
    // Response sequence: first call returns a tool_use response,
    // second call returns a final text response.
    QList<LLMMessage> responseSequence;
    int callCount = 0;

    void chat(const QList<LLMMessage> & /*messages*/,
              std::function<void(LLMMessage)> onMessage,
              std::function<void()> onComplete,
              std::function<void(QString, QString)> /*onError*/) override
    {
        ++callCount;
        if (callCount <= responseSequence.size())
        {
            onMessage(responseSequence[callCount - 1]);
            onComplete();
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
// Simple read-only tools for end-to-end scenarios
// ============================================================

class ReadFileTool : public ITool
{
public:
    QMap<QString, QString> files;

    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("file_read");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("Read a file");
    }
    [[nodiscard]] QJsonObject schema() const override { return QJsonObject{}; }

    ToolResult execute(const QJsonObject &params) override
    {
        QString path = params.value(QStringLiteral("path")).toString();
        auto it = files.find(path);
        if (it == files.end())
            return ToolResult::err(QStringLiteral("FILE_NOT_FOUND"),
                                  QStringLiteral("File not found: %1").arg(path));
        return ToolResult::ok(it.value());
    }

    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Read;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

class GrepTool : public ITool
{
public:
    QMap<QString, QString> files;

    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("grep");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("Search for a pattern in files");
    }
    [[nodiscard]] QJsonObject schema() const override { return QJsonObject{}; }

    ToolResult execute(const QJsonObject &params) override
    {
        QString pattern = params.value(QStringLiteral("pattern")).toString();
        QString path = params.value(QStringLiteral("path")).toString();
        auto it = files.find(path);
        if (it == files.end())
            return ToolResult::err(QStringLiteral("FILE_NOT_FOUND"),
                                  QStringLiteral("File not found"));

        QStringList matches;
        QStringList lines = it.value().split('\n');
        for (const auto &line : lines)
        {
            if (line.contains(pattern))
                matches.append(line);
        }

        if (matches.isEmpty())
            return ToolResult::ok(QStringLiteral("(no matches)"));
        return ToolResult::ok(matches.join('\n'));
    }

    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Read;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

// ============================================================
// Mock shell exec tool (requires Exec permission)
// ============================================================

class ShellExecTool : public ITool
{
public:
    QString lastCommand;
    QString simulatedOutput = QStringLiteral("Command executed successfully");

    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("shell_exec");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("Execute a shell command");
    }
    [[nodiscard]] QJsonObject schema() const override { return QJsonObject{}; }

    ToolResult execute(const QJsonObject &params) override
    {
        lastCommand = params.value(QStringLiteral("command")).toString();
        return ToolResult::ok(simulatedOutput);
    }

    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Exec;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

// ============================================================
// Test Fixture
// ============================================================

class EndToEndCliTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        engine = std::make_unique<ScenarioEngine>();
        registry = std::make_unique<ToolRegistry>();
        permissions = std::make_unique<PermissionManager>();
        context = std::make_unique<ContextManager>();

        readTool = new ReadFileTool();
        grepTool = new GrepTool();
        execTool = new ShellExecTool();

        readTool->files = {
            {QStringLiteral("main.cpp"),
             QStringLiteral("#include <iostream>\nint main() { return 0; }")},
            {QStringLiteral("README.md"),
             QStringLiteral("# My Project\n\nA sample project for testing.")},
        };

        grepTool->files = readTool->files;
        execTool->simulatedOutput = QStringLiteral("Command executed successfully");

        registry->registerTool(std::unique_ptr<ITool>(readTool));
        registry->registerTool(std::unique_ptr<ITool>(grepTool));
        registry->registerTool(std::unique_ptr<ITool>(execTool));

        // Auto-approve read-level tools
        permissions->setAutoApproved(PermissionLevel::Read, true);

        repl = std::make_unique<CliRepl>(
            *engine, *registry, *permissions, *context);

        capturedLines.clear();
        QObject::connect(repl.get(), &CliRepl::outputLine,
                         [this](const QString &line)
                         { capturedLines.append(line); });
        QObject::connect(repl.get(), &CliRepl::jsonEvent,
                         [this](const QString &line)
                         { capturedLines.append(line); });
    }

    void TearDown() override
    {
        repl.reset();
        context.reset();
        permissions.reset();
        registry.reset();
        engine.reset();
    }

    // Helper: build a tool-use LLM response
    static LLMMessage toolUseResponse(const QString &toolName,
                                      const QString &toolId,
                                      const QJsonObject &params)
    {
        LLMMessage msg;
        msg.role = MessageRole::Assistant;
        msg.content = QString();
        ToolCall call;
        call.id = toolId;
        call.name = toolName;
        call.params = params;
        msg.toolCalls.append(call);
        return msg;
    }

    // Helper: build a final text LLM response
    static LLMMessage textResponse(const QString &text)
    {
        LLMMessage msg;
        msg.role = MessageRole::Assistant;
        msg.content = text;
        return msg;
    }

    std::unique_ptr<ScenarioEngine> engine;
    std::unique_ptr<ToolRegistry> registry;
    std::unique_ptr<PermissionManager> permissions;
    std::unique_ptr<ContextManager> context;
    std::unique_ptr<CliRepl> repl;
    QStringList capturedLines;

    // Raw pointers for setup (ownership transferred to registry)
    ReadFileTool *readTool = nullptr;
    GrepTool *grepTool = nullptr;
    ShellExecTool *execTool = nullptr;
};

// ============================================================
// E2E-1: User asks to read a file — full tool-use cycle
// ============================================================

TEST_F(EndToEndCliTest, ReadFileEndToEnd)
{
    // Scenario: LLM receives "show me main.cpp", calls file_read,
    // gets content, then summarizes.
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("main.cpp");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("file_read"), QStringLiteral("call_1"), params),
        textResponse(
            QStringLiteral("The file contains a simple main function "
                           "that returns 0.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Human);
    auto state = repl->processInput(QStringLiteral("show me main.cpp"));

    EXPECT_EQ(state, TaskState::Completed);
    EXPECT_EQ(engine->callCount, 2);

    // The prompt line should appear
    EXPECT_TRUE(capturedLines.contains(QStringLiteral("> show me main.cpp")));
}

// ============================================================
// E2E-2: User asks to search — grep tool cycle
// ============================================================

TEST_F(EndToEndCliTest, GrepEndToEnd)
{
    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("#include");
    params[QStringLiteral("path")] = QStringLiteral("main.cpp");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("grep"), QStringLiteral("call_1"), params),
        textResponse(
            QStringLiteral("Found #include <iostream> in main.cpp.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Human);
    auto state = repl->processInput(
        QStringLiteral("search for #include in main.cpp"));

    EXPECT_EQ(state, TaskState::Completed);
    EXPECT_EQ(engine->callCount, 2);
}

// ============================================================
// E2E-3: Multi-step — tool call then another tool call
// ============================================================

TEST_F(EndToEndCliTest, MultiStepToolCalls)
{
    QJsonObject readParams;
    readParams[QStringLiteral("path")] = QStringLiteral("main.cpp");

    QJsonObject grepParams;
    grepParams[QStringLiteral("pattern")] = QStringLiteral("int");
    grepParams[QStringLiteral("path")] = QStringLiteral("main.cpp");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("file_read"), QStringLiteral("call_1"), readParams),
        toolUseResponse(
            QStringLiteral("grep"), QStringLiteral("call_2"), grepParams),
        textResponse(
            QStringLiteral("Found 'int main()' with a return 0.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Human);
    auto state = repl->processInput(
        QStringLiteral("analyze main.cpp for function signatures"));

    EXPECT_EQ(state, TaskState::Completed);
    EXPECT_EQ(engine->callCount, 3);
}

// ============================================================
// E2E-4: Batch mode — multiple inputs processed sequentially
// ============================================================

TEST_F(EndToEndCliTest, BatchModeMultipleTurns)
{
    QJsonObject params1;
    params1[QStringLiteral("path")] = QStringLiteral("main.cpp");

    QJsonObject params2;
    params2[QStringLiteral("path")] = QStringLiteral("README.md");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("file_read"), QStringLiteral("c1"), params1),
        textResponse(QStringLiteral("main.cpp content shown.")),
        toolUseResponse(
            QStringLiteral("file_read"), QStringLiteral("c2"), params2),
        textResponse(QStringLiteral("README.md content shown.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Human);
    repl->processBatch({
        QStringLiteral("read main.cpp"),
        QStringLiteral("read README.md"),
    });

    EXPECT_EQ(engine->callCount, 4);
}

// ============================================================
// E2E-5: JSON mode — output is valid JSON Lines
// ============================================================

TEST_F(EndToEndCliTest, JsonModeEndToEnd)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("README.md");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("file_read"), QStringLiteral("call_1"), params),
        textResponse(QStringLiteral("The README describes My Project.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Json);
    repl->processInput(QStringLiteral("what is the project about?"));

    EXPECT_EQ(engine->callCount, 2);

    // All output lines must be valid JSON
    for (const auto &line : capturedLines)
    {
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        EXPECT_FALSE(doc.isNull())
            << "Invalid JSON line: " << line.toStdString();
    }
}

// ============================================================
// E2E-6: Error recovery — tool not found
// ============================================================

TEST_F(EndToEndCliTest, ToolNotFoundRecovery)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("nope.txt");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("nonexistent_tool"), QStringLiteral("c1"), params),
        textResponse(
            QStringLiteral("Sorry, I tried an unknown tool. "
                           "Let me try again differently.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Human);
    auto state = repl->processInput(
        QStringLiteral("read a file"));

    // Agent should still complete — LLM recovers from tool error
    EXPECT_EQ(state, TaskState::Completed);
    EXPECT_EQ(engine->callCount, 2);
}

// ============================================================
// E2E-7: Context accumulates across turns
// ============================================================

TEST_F(EndToEndCliTest, ContextAccumulatesAcrossTurns)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("main.cpp");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("file_read"), QStringLiteral("c1"), params),
        textResponse(QStringLiteral("Here is main.cpp.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Human);
    repl->processInput(QStringLiteral("first question"));
    EXPECT_EQ(engine->callCount, 2); // tool_use + final response

    // Second turn — engine called with accumulated context
    engine->callCount = 0;
    engine->responseSequence = {
        textResponse(QStringLiteral("Based on the previous context...")),
    };
    repl->processInput(QStringLiteral("follow up question"));
    EXPECT_EQ(engine->callCount, 1);
}

// ============================================================
// E2E-8: Reset clears context
// ============================================================

TEST_F(EndToEndCliTest, ResetClearsContext)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("README.md");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("file_read"), QStringLiteral("c1"), params),
        textResponse(QStringLiteral("Content shown.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Human);
    repl->processInput(QStringLiteral("read README"));

    // Reset
    repl->processInput(QStringLiteral("/reset"));
    EXPECT_TRUE(capturedLines.contains(
        QStringLiteral("[System] Conversation reset.")));

    // After reset, a new message should work
    engine->callCount = 0;
    engine->responseSequence = {
        textResponse(QStringLiteral("Fresh start.")),
    };
    auto state = repl->processInput(QStringLiteral("new question"));
    EXPECT_EQ(state, TaskState::Completed);
}

// ============================================================
// E2E-9: Permission confirmation — user approves
// ============================================================

TEST_F(EndToEndCliTest, PermissionConfirmationApproved)
{
    // Set up shell exec tool (requires permission)
    auto *execTool = new ShellExecTool();
    registry->registerTool(std::unique_ptr<ITool>(execTool));

    // Set up permission callback that auto-approves
    permissions->setPermissionCallback([](const PermissionRequest &request) {
        // Simulate user typing "y"
        return request.toolName == QLatin1String("shell_exec");
    });

    QJsonObject params;
    params[QStringLiteral("command")] = QStringLiteral("echo hello");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("shell_exec"), QStringLiteral("c1"), params),
        textResponse(QStringLiteral("Command executed successfully.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Human);
    auto state = repl->processInput(QStringLiteral("run echo hello"));

    EXPECT_EQ(state, TaskState::Completed);
    EXPECT_EQ(execTool->lastCommand, QStringLiteral("echo hello"));
}

TEST_F(EndToEndCliTest, PermissionConfirmationDenied)
{
    // Set up shell exec tool (requires permission)
    auto *execTool = new ShellExecTool();
    registry->registerTool(std::unique_ptr<ITool>(execTool));

    // Set up permission callback that denies
    permissions->setPermissionCallback([](const PermissionRequest &) {
        return false; // Deny all
    });

    QJsonObject params;
    params[QStringLiteral("command")] = QStringLiteral("rm -rf /");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("shell_exec"), QStringLiteral("c1"), params),
        textResponse(QStringLiteral("I cannot execute that command.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Human);
    auto state = repl->processInput(QStringLiteral("delete everything"));

    // Agent should still complete (with denied tool result)
    EXPECT_EQ(state, TaskState::Completed);
    // Tool should not have been executed
    EXPECT_TRUE(execTool->lastCommand.isEmpty());
}

TEST_F(EndToEndCliTest, PermissionAutoApprovedForRead)
{
    // Read-level tools should be auto-approved without callback
    bool callbackCalled = false;
    permissions->setPermissionCallback([&callbackCalled](const PermissionRequest &) {
        callbackCalled = true;
        return true;
    });

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("main.cpp");

    engine->responseSequence = {
        toolUseResponse(
            QStringLiteral("file_read"), QStringLiteral("c1"), params),
        textResponse(QStringLiteral("Here's main.cpp.")),
    };

    repl->setOutputMode(CliRepl::OutputMode::Human);
    auto state = repl->processInput(QStringLiteral("read main.cpp"));

    EXPECT_EQ(state, TaskState::Completed);
    // Callback should NOT have been called (auto-approved)
    EXPECT_FALSE(callbackCalled);
}
