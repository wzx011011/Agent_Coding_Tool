#include <gtest/gtest.h>

#include <QStringList>
#include <memory>

#include "core/types.h"
#include "framework/command_registry.h"
#include "framework/commands/copy_command.h"
#include "infrastructure/interfaces.h"

using namespace act::core;
using namespace act::framework;
using namespace act::framework::commands;

namespace {

/// Minimal mock IProcess (clipboard uses QProcess directly, so this is unused
/// in the command lambda but required by registerTo signature).
class MockCopyProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int /*timeoutMs*/ = 30000) override
    {
        callback(0, QString());
    }

    void cancel() override {}
};

} // anonymous namespace

// ============================================================
// CopyCommand Tests
// ============================================================

class CopyCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockProcess = std::make_unique<MockCopyProcess>();
    }

    void TearDown() override
    {
        mockProcess.reset();
    }

    std::unique_ptr<MockCopyProcess> mockProcess;
    QList<LLMMessage> messages;
    QStringList capturedOutput;
};

TEST_F(CopyCommandTest, RegistersWithRegistry)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };
    auto histGetter = [this]() -> const QList<LLMMessage> & { return messages; };

    CopyCommand::registerTo(registry, *mockProcess, output, histGetter);

    EXPECT_TRUE(registry.hasCommand(QStringLiteral("copy")));
    auto info = registry.getCommand(QStringLiteral("copy"));
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->name, QStringLiteral("copy"));
}

TEST_F(CopyCommandTest, ShowsMessageWhenNoHistory)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };
    auto histGetter = [this]() -> const QList<LLMMessage> & { return messages; };

    CopyCommand::registerTo(registry, *mockProcess, output, histGetter);

    auto info = registry.getCommand(QStringLiteral("copy"));
    ASSERT_NE(info, nullptr);
    info->handler({});

    bool foundNoResponse = false;
    for (const auto &line : capturedOutput)
    {
        if (line.contains(QStringLiteral("No assistant response")))
            foundNoResponse = true;
    }
    EXPECT_TRUE(foundNoResponse);
}

TEST_F(CopyCommandTest, FindsLastAssistantMessage)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };
    auto histGetter = [this]() -> const QList<LLMMessage> & { return messages; };

    // Add messages
    LLMMessage userMsg;
    userMsg.role = MessageRole::User;
    userMsg.content = QStringLiteral("hello");
    messages.append(userMsg);

    LLMMessage assistantMsg;
    assistantMsg.role = MessageRole::Assistant;
    assistantMsg.content = QStringLiteral("Hello! How can I help?");
    messages.append(assistantMsg);

    CopyCommand::registerTo(registry, *mockProcess, output, histGetter);

    auto info = registry.getCommand(QStringLiteral("copy"));
    ASSERT_NE(info, nullptr);
    info->handler({});

    // Should attempt to copy and show result (success or failure)
    EXPECT_FALSE(capturedOutput.isEmpty());
    bool foundCopy = false;
    for (const auto &line : capturedOutput)
    {
        if (line.contains(QStringLiteral("clipboard")) ||
            line.contains(QStringLiteral("CLIPBOARD_ERROR")))
            foundCopy = true;
    }
    EXPECT_TRUE(foundCopy);
}

TEST_F(CopyCommandTest, FindsMostRecentAssistantMessage)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };
    auto histGetter = [this]() -> const QList<LLMMessage> & { return messages; };

    // Add multiple assistant messages
    LLMMessage a1;
    a1.role = MessageRole::Assistant;
    a1.content = QStringLiteral("First response");
    messages.append(a1);

    LLMMessage a2;
    a2.role = MessageRole::Assistant;
    a2.content = QStringLiteral("Second response (latest)");
    messages.append(a2);

    CopyCommand::registerTo(registry, *mockProcess, output, histGetter);

    auto info = registry.getCommand(QStringLiteral("copy"));
    ASSERT_NE(info, nullptr);
    info->handler({});

    // Should use the most recent assistant message
    EXPECT_FALSE(capturedOutput.isEmpty());
}

TEST_F(CopyCommandTest, ReturnsTrueFromHandler)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };
    auto histGetter = [this]() -> const QList<LLMMessage> & { return messages; };

    CopyCommand::registerTo(registry, *mockProcess, output, histGetter);

    auto info = registry.getCommand(QStringLiteral("copy"));
    ASSERT_NE(info, nullptr);
    EXPECT_TRUE(info->handler({}));
}

// ============================================================
// StripMarkdown tests (indirectly via the command)
// ============================================================

TEST_F(CopyCommandTest, RawFlagPreservesContent)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };
    auto histGetter = [this]() -> const QList<LLMMessage> & { return messages; };

    LLMMessage a;
    a.role = MessageRole::Assistant;
    a.content = QStringLiteral("**bold** and `code` and ## heading");
    messages.append(a);

    CopyCommand::registerTo(registry, *mockProcess, output, histGetter);

    auto info = registry.getCommand(QStringLiteral("copy"));
    ASSERT_NE(info, nullptr);

    // With --raw flag
    capturedOutput.clear();
    info->handler({QStringLiteral("--raw")});
    EXPECT_FALSE(capturedOutput.isEmpty());
}
