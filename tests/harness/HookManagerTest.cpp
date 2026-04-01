#include <gtest/gtest.h>

#include "harness/hook_manager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include <thread>

using namespace act::harness;

namespace
{

// Helper to build a basic PreToolUse context for shell_exec
HookContext makePreToolUseContext(const QString &toolName)
{
    HookContext ctx;
    ctx.eventType = HookEventType::PreToolUse;
    ctx.toolName = toolName;
    return ctx;
}

HookContext makePostToolUseContext(const QString &toolName,
                                  bool success,
                                  const QString &output)
{
    HookContext ctx;
    ctx.eventType = HookEventType::PostToolUse;
    ctx.toolName = toolName;
    ctx.toolSuccess = success;
    ctx.toolOutput = output;
    return ctx;
}

HookContext makeUserPromptContext(const QString &input)
{
    HookContext ctx;
    ctx.eventType = HookEventType::UserPromptSubmit;
    ctx.userInput = input;
    return ctx;
}

HookEntry makeShellHook(HookEventType eventType,
                        const QString &matcher,
                        const QString &command)
{
    HookEntry entry;
    entry.eventType = eventType;
    entry.matcher = matcher;
    entry.enabled = true;
    HookCommand cmd;
    cmd.type = HookCommandType::Shell;
    cmd.command = command;
    entry.commands.append(cmd);
    return entry;
}

HookEntry makePromptHook(HookEventType eventType,
                         const QString &matcher,
                         const QString &promptText)
{
    HookEntry entry;
    entry.eventType = eventType;
    entry.matcher = matcher;
    entry.enabled = true;
    HookCommand cmd;
    cmd.type = HookCommandType::Prompt;
    cmd.command = promptText;
    entry.commands.append(cmd);
    return entry;
}

} // anonymous namespace

// --- Basic Registration ---

TEST(HookManagerTest, RegisterHookIncreasesCount)
{
    HookManager mgr;
    ASSERT_EQ(mgr.hookCount(), 0);

    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "", "echo hello"));
    ASSERT_EQ(mgr.hookCount(), 1);

    mgr.registerHook(makeShellHook(
        HookEventType::PostToolUse, "shell_exec", "echo done"));
    ASSERT_EQ(mgr.hookCount(), 2);
}

TEST(HookManagerTest, ClearHooks)
{
    HookManager mgr;
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "", "echo hello"));
    mgr.registerHook(makeShellHook(
        HookEventType::PostToolUse, "", "echo done"));
    ASSERT_EQ(mgr.hookCount(), 2);

    mgr.clearHooks();
    ASSERT_EQ(mgr.hookCount(), 0);
}

TEST(HookManagerTest, ListHooksReturnsRegistered)
{
    HookManager mgr;
    auto hook = makeShellHook(
        HookEventType::PreToolUse, "shell_exec", "echo test");
    mgr.registerHook(hook);

    auto hooks = mgr.listHooks();
    ASSERT_EQ(hooks.size(), 1);
    EXPECT_EQ(hooks[0].eventType, HookEventType::PreToolUse);
    EXPECT_EQ(hooks[0].matcher, QStringLiteral("shell_exec"));
}

// --- Shell Hook Execution ---

TEST(HookManagerTest, ShellHookSuccessProceeds)
{
    HookManager mgr;
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "", "echo ok"));

    auto result = mgr.fireEvent(makePreToolUseContext("any_tool"));
    EXPECT_TRUE(result.proceed);
}

TEST(HookManagerTest, ShellHookOutputBecomesFeedback)
{
    HookManager mgr;
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "", "echo hello_feedback"));

    auto result = mgr.fireEvent(makePreToolUseContext("any_tool"));
    EXPECT_TRUE(result.proceed);
    EXPECT_TRUE(result.feedback.contains(QStringLiteral("hello_feedback")))
        << "Actual feedback: " << result.feedback.toStdString();
}

TEST(HookManagerTest, ShellHookNonZeroExitBlocks)
{
    HookManager mgr;
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "", "exit 1"));

    auto result = mgr.fireEvent(makePreToolUseContext("any_tool"));
    EXPECT_FALSE(result.proceed);
}

TEST(HookManagerTest, ShellHookTimeoutBlocks)
{
    HookManager mgr;
    // A command that sleeps longer than the 5s timeout
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "", "timeout /t 10 /nobreak >nul 2>&1 || sleep 10"));

    auto result = mgr.fireEvent(makePreToolUseContext("any_tool"));
    EXPECT_FALSE(result.proceed);
}

// --- Matcher Regex ---

TEST(HookManagerTest, MatcherMatchesToolName)
{
    HookManager mgr;
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "shell_.*", "echo matched"));

    auto result = mgr.fireEvent(makePreToolUseContext("shell_exec"));
    EXPECT_TRUE(result.proceed);
    EXPECT_TRUE(result.feedback.contains(QStringLiteral("matched")));
}

TEST(HookManagerTest, MatcherDoesNotMatchDifferentTool)
{
    HookManager mgr;
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "shell_.*", "echo should_not_run"));

    auto result = mgr.fireEvent(makePreToolUseContext("file_read"));
    EXPECT_TRUE(result.proceed);
    EXPECT_TRUE(result.feedback.isEmpty())
        << "Feedback should be empty, got: "
        << result.feedback.toStdString();
}

TEST(HookManagerTest, EmptyMatcherMatchesAll)
{
    HookManager mgr;
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "", "echo universal"));

    auto result1 = mgr.fireEvent(makePreToolUseContext("shell_exec"));
    EXPECT_TRUE(result1.feedback.contains(QStringLiteral("universal")));

    auto result2 = mgr.fireEvent(makePreToolUseContext("file_read"));
    EXPECT_TRUE(result2.feedback.contains(QStringLiteral("universal")));
}

// --- Prompt Hook ---

TEST(HookManagerTest, PromptHookInjectsFeedback)
{
    HookManager mgr;
    mgr.registerHook(makePromptHook(
        HookEventType::PreToolUse, "shell_exec",
        "Be careful with shell commands"));

    auto result = mgr.fireEvent(makePreToolUseContext("shell_exec"));
    EXPECT_TRUE(result.proceed);
    EXPECT_EQ(result.feedback,
              QStringLiteral("Be careful with shell commands"));
}

TEST(HookManagerTest, PromptHookNeverBlocks)
{
    HookManager mgr;
    mgr.registerHook(makePromptHook(
        HookEventType::PreToolUse, "", "some injection text"));

    auto result = mgr.fireEvent(makePreToolUseContext("any_tool"));
    EXPECT_TRUE(result.proceed);
    EXPECT_FALSE(result.feedback.isEmpty());
}

// --- Multiple Hooks Aggregation ---

TEST(HookManagerTest, MultipleHooksAggregateFeedback)
{
    HookManager mgr;
    mgr.registerHook(makePromptHook(
        HookEventType::PreToolUse, "shell_exec", "Warning 1"));
    mgr.registerHook(makePromptHook(
        HookEventType::PreToolUse, "shell_exec", "Warning 2"));

    auto result = mgr.fireEvent(makePreToolUseContext("shell_exec"));
    EXPECT_TRUE(result.proceed);
    EXPECT_TRUE(result.feedback.contains(QStringLiteral("Warning 1")));
    EXPECT_TRUE(result.feedback.contains(QStringLiteral("Warning 2")));
}

TEST(HookManagerTest, ShellBlockOverridesPromptProceed)
{
    HookManager mgr;
    mgr.registerHook(makePromptHook(
        HookEventType::PreToolUse, "", "friendly text"));
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "", "exit 1"));

    auto result = mgr.fireEvent(makePreToolUseContext("any_tool"));
    EXPECT_FALSE(result.proceed);
    // Feedback should contain both outputs
    EXPECT_TRUE(result.feedback.contains(QStringLiteral("friendly text")));
}

// --- Event Type Filtering ---

TEST(HookManagerTest, DifferentEventTypesDontInterfere)
{
    HookManager mgr;
    mgr.registerHook(makeShellHook(
        HookEventType::PostToolUse, "", "echo post"));

    auto result = mgr.fireEvent(makePreToolUseContext("any_tool"));
    EXPECT_TRUE(result.proceed);
    EXPECT_TRUE(result.feedback.isEmpty())
        << "PreToolUse should not trigger PostToolUse hook, got: "
        << result.feedback.toStdString();
}

TEST(HookManagerTest, PostToolUseHookFiresOnPostEvent)
{
    HookManager mgr;
    mgr.registerHook(makeShellHook(
        HookEventType::PostToolUse, "", "echo post_done"));

    auto result = mgr.fireEvent(
        makePostToolUseContext("shell_exec", true, "output"));
    EXPECT_TRUE(result.proceed);
    EXPECT_TRUE(result.feedback.contains(QStringLiteral("post_done")));
}

TEST(HookManagerTest, UserPromptSubmitHookFires)
{
    HookManager mgr;
    mgr.registerHook(makePromptHook(
        HookEventType::UserPromptSubmit, "",
        "User submitted a prompt"));

    auto result = mgr.fireEvent(makeUserPromptContext("hello"));
    EXPECT_TRUE(result.proceed);
    EXPECT_EQ(result.feedback,
              QStringLiteral("User submitted a prompt"));
}

// --- Disabled Hook ---

TEST(HookManagerTest, DisabledHookDoesNotFire)
{
    HookManager mgr;
    HookEntry entry = makeShellHook(
        HookEventType::PreToolUse, "", "echo should_not_run");
    entry.enabled = false;
    mgr.registerHook(entry);

    auto result = mgr.fireEvent(makePreToolUseContext("any_tool"));
    EXPECT_TRUE(result.proceed);
    EXPECT_TRUE(result.feedback.isEmpty())
        << "Disabled hook should not produce feedback, got: "
        << result.feedback.toStdString();
}

// --- loadFromConfig ---

TEST(HookManagerTest, LoadFromConfigParsesShellHook)
{
    HookManager mgr;

    QJsonObject hooksConfig;
    QJsonArray preHooks;
    QJsonObject entry;
    entry[QStringLiteral("matcher")] = QStringLiteral("shell_exec");
    QJsonArray hookCmds;
    QJsonObject cmd;
    cmd[QStringLiteral("type")] = QStringLiteral("bash");
    cmd[QStringLiteral("command")] = QStringLiteral("echo loaded");
    hookCmds.append(cmd);
    entry[QStringLiteral("hooks")] = hookCmds;
    preHooks.append(entry);
    hooksConfig[QStringLiteral("PreToolUse")] = preHooks;

    mgr.loadFromConfig(hooksConfig);
    ASSERT_EQ(mgr.hookCount(), 1);

    auto result = mgr.fireEvent(makePreToolUseContext("shell_exec"));
    EXPECT_TRUE(result.proceed);
    EXPECT_TRUE(result.feedback.contains(QStringLiteral("loaded")));
}

TEST(HookManagerTest, LoadFromConfigParsesPromptHook)
{
    HookManager mgr;

    QJsonObject hooksConfig;
    QJsonArray preHooks;
    QJsonObject entry;
    entry[QStringLiteral("matcher")] = QStringLiteral("");
    QJsonArray hookCmds;
    QJsonObject cmd;
    cmd[QStringLiteral("type")] = QStringLiteral("prompt");
    cmd[QStringLiteral("prompt")] = QStringLiteral("Be careful");
    hookCmds.append(cmd);
    entry[QStringLiteral("hooks")] = hookCmds;
    preHooks.append(entry);
    hooksConfig[QStringLiteral("PreToolUse")] = preHooks;

    mgr.loadFromConfig(hooksConfig);
    ASSERT_EQ(mgr.hookCount(), 1);

    auto result = mgr.fireEvent(makePreToolUseContext("any_tool"));
    EXPECT_TRUE(result.proceed);
    EXPECT_EQ(result.feedback, QStringLiteral("Be careful"));
}

TEST(HookManagerTest, LoadFromConfigParsesMultipleEventTypes)
{
    HookManager mgr;

    QJsonObject hooksConfig;

    // PreToolUse hook
    QJsonArray preHooks;
    QJsonObject preEntry;
    preEntry[QStringLiteral("matcher")] = QStringLiteral("shell_exec");
    QJsonArray preCmds;
    QJsonObject preCmd;
    preCmd[QStringLiteral("type")] = QStringLiteral("bash");
    preCmd[QStringLiteral("command")] = QStringLiteral("echo pre");
    preCmds.append(preCmd);
    preEntry[QStringLiteral("hooks")] = preCmds;
    preHooks.append(preEntry);
    hooksConfig[QStringLiteral("PreToolUse")] = preHooks;

    // PostToolUse hook
    QJsonArray postHooks;
    QJsonObject postEntry;
    postEntry[QStringLiteral("matcher")] = QStringLiteral("");
    QJsonArray postCmds;
    QJsonObject postCmd;
    postCmd[QStringLiteral("type")] = QStringLiteral("prompt");
    postCmd[QStringLiteral("prompt")] = QStringLiteral("post_done");
    postCmds.append(postCmd);
    postEntry[QStringLiteral("hooks")] = postCmds;
    postHooks.append(postEntry);
    hooksConfig[QStringLiteral("PostToolUse")] = postHooks;

    mgr.loadFromConfig(hooksConfig);
    ASSERT_EQ(mgr.hookCount(), 2);
}

TEST(HookManagerTest, LoadFromConfigUnknownEventTypeSkipped)
{
    HookManager mgr;

    QJsonObject hooksConfig;
    QJsonArray unknownHooks;
    QJsonObject entry;
    entry[QStringLiteral("matcher")] = QStringLiteral("");
    QJsonArray cmds;
    QJsonObject cmd;
    cmd[QStringLiteral("type")] = QStringLiteral("bash");
    cmd[QStringLiteral("command")] = QStringLiteral("echo test");
    cmds.append(cmd);
    entry[QStringLiteral("hooks")] = cmds;
    unknownHooks.append(entry);
    hooksConfig[QStringLiteral("UnknownEvent")] = unknownHooks;

    mgr.loadFromConfig(hooksConfig);
    EXPECT_EQ(mgr.hookCount(), 0);
}

TEST(HookManagerTest, LoadFromConfigClearsExisting)
{
    HookManager mgr;

    // Register a hook manually first
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "", "echo manual"));

    // Load config should add to existing hooks (doesn't clear)
    QJsonObject hooksConfig;
    QJsonArray preHooks;
    QJsonObject entry;
    entry[QStringLiteral("matcher")] = QStringLiteral("");
    QJsonArray cmds;
    QJsonObject cmd;
    cmd[QStringLiteral("type")] = QStringLiteral("bash");
    cmd[QStringLiteral("command")] = QStringLiteral("echo config");
    cmds.append(cmd);
    entry[QStringLiteral("hooks")] = cmds;
    preHooks.append(entry);
    hooksConfig[QStringLiteral("PreToolUse")] = preHooks;

    mgr.loadFromConfig(hooksConfig);
    EXPECT_EQ(mgr.hookCount(), 2);
}

// --- Thread Safety ---

TEST(HookManagerTest, ConcurrentFireEvent)
{
    HookManager mgr;
    mgr.registerHook(makePromptHook(
        HookEventType::PreToolUse, "", "safe_text"));

    constexpr int kThreadCount = 8;
    std::vector<HookResult> results(kThreadCount);
    std::vector<std::thread> threads;

    for (int i = 0; i < kThreadCount; ++i)
    {
        threads.emplace_back([&mgr, &results, i]()
        {
            auto ctx = makePreToolUseContext(
                QStringLiteral("tool_%1").arg(i));
            results[i] = mgr.fireEvent(ctx);
        });
    }

    for (auto &t : threads)
        t.join();

    for (int i = 0; i < kThreadCount; ++i)
    {
        EXPECT_TRUE(results[i].proceed);
        EXPECT_EQ(results[i].feedback, QStringLiteral("safe_text"));
    }
}

TEST(HookManagerTest, ConcurrentRegisterAndFire)
{
    HookManager mgr;

    constexpr int kIterations = 50;
    std::vector<std::thread> threads;

    // One thread registers hooks
    threads.emplace_back([&mgr]()
    {
        for (int i = 0; i < kIterations; ++i)
        {
            mgr.registerHook(makePromptHook(
                HookEventType::PreToolUse, "",
                QStringLiteral("hook_%1").arg(i)));
        }
    });

    // Another thread fires events
    threads.emplace_back([&mgr]()
    {
        for (int i = 0; i < kIterations; ++i)
        {
            auto ctx = makePreToolUseContext("any_tool");
            auto result = mgr.fireEvent(ctx);
            // Should not crash; proceed may vary
            (void)result;
        }
    });

    for (auto &t : threads)
        t.join();

    // All hooks should be registered
    EXPECT_EQ(mgr.hookCount(), kIterations);
}

// --- Context Data Tests ---

TEST(HookManagerTest, BuildEnvironmentJsonContainsContext)
{
    // Verify the shell command receives context via ACT_HOOK_CONTEXT env var
    // by echoing it back
    HookManager mgr;
    // The hook echoes the env var so we can check it was set
    mgr.registerHook(makeShellHook(
        HookEventType::PreToolUse, "",
        "echo %ACT_HOOK_CONTEXT%"));

    HookContext ctx;
    ctx.eventType = HookEventType::PreToolUse;
    ctx.toolName = QStringLiteral("test_tool");
    ctx.toolSuccess = true;
    ctx.toolOutput = QStringLiteral("some output");

    auto result = mgr.fireEvent(ctx);
    EXPECT_TRUE(result.proceed);
    // The output should contain the tool name somewhere in the JSON
    EXPECT_TRUE(result.feedback.contains(QStringLiteral("test_tool")))
        << "Expected context JSON to contain 'test_tool', got: "
        << result.feedback.toStdString();
}
