#include <gtest/gtest.h>

#include <QStringList>
#include <memory>

#include "framework/command_registry.h"
#include "framework/commands/diff_command.h"
#include "infrastructure/interfaces.h"

using namespace act::framework;
using namespace act::framework::commands;

namespace {

/// Mock IProcess that captures git diff arguments.
class MockDiffProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int /*timeoutMs*/ = 30000) override
    {
        lastCommand = command;
        lastArgs = args;

        if (simulateError)
        {
            callback(128, QStringLiteral("fatal: not a git repository"));
            return;
        }

        // Return a sample diff output
        callback(0, QStringLiteral(
            "diff --git a/test.txt b/test.txt\n"
            "index 1234567..abcdef 100644\n"
            "--- a/test.txt\n"
            "+++ b/test.txt\n"
            "@@ -1,3 +1,4 @@\n"
            " line1\n"
            "-old line\n"
            "+new line\n"
            "+extra line\n"));
    }

    void cancel() override {}

    QString lastCommand;
    QStringList lastArgs;
    bool simulateError = false;
};

} // anonymous namespace

// ============================================================
// DiffCommand Tests
// ============================================================

class DiffCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockProcess = std::make_unique<MockDiffProcess>();
    }

    void TearDown() override
    {
        mockProcess.reset();
    }

    std::unique_ptr<MockDiffProcess> mockProcess;
    QStringList capturedOutput;
};

TEST_F(DiffCommandTest, RegistersWithRegistry)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DiffCommand::registerTo(registry, *mockProcess, output);

    EXPECT_TRUE(registry.hasCommand(QStringLiteral("diff")));
    auto info = registry.getCommand(QStringLiteral("diff"));
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->name, QStringLiteral("diff"));
}

TEST_F(DiffCommandTest, NoArgsRunsGitDiff)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DiffCommand::registerTo(registry, *mockProcess, output);
    registry.execute(QStringLiteral("diff"), {});

    EXPECT_EQ(mockProcess->lastCommand, QStringLiteral("git"));
    ASSERT_GE(mockProcess->lastArgs.size(), 1);
    EXPECT_EQ(mockProcess->lastArgs.at(0), QStringLiteral("diff"));
}

TEST_F(DiffCommandTest, StagedFlagPassesToGit)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DiffCommand::registerTo(registry, *mockProcess, output);
    registry.execute(QStringLiteral("diff"), {QStringLiteral("--staged")});

    EXPECT_EQ(mockProcess->lastCommand, QStringLiteral("git"));
    ASSERT_GE(mockProcess->lastArgs.size(), 2);
    EXPECT_EQ(mockProcess->lastArgs.at(0), QStringLiteral("diff"));
    EXPECT_EQ(mockProcess->lastArgs.at(1), QStringLiteral("--staged"));
}

TEST_F(DiffCommandTest, StatFlagPassesToGit)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DiffCommand::registerTo(registry, *mockProcess, output);
    registry.execute(QStringLiteral("diff"), {QStringLiteral("--stat")});

    EXPECT_EQ(mockProcess->lastCommand, QStringLiteral("git"));
    ASSERT_GE(mockProcess->lastArgs.size(), 2);
    EXPECT_EQ(mockProcess->lastArgs.at(0), QStringLiteral("diff"));
    EXPECT_EQ(mockProcess->lastArgs.at(1), QStringLiteral("--stat"));
}

TEST_F(DiffCommandTest, FilePathPassesToGit)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DiffCommand::registerTo(registry, *mockProcess, output);
    registry.execute(QStringLiteral("diff"), {QStringLiteral("src/main.cpp")});

    EXPECT_EQ(mockProcess->lastCommand, QStringLiteral("git"));
    ASSERT_GE(mockProcess->lastArgs.size(), 2);
    EXPECT_EQ(mockProcess->lastArgs.at(0), QStringLiteral("diff"));
    EXPECT_EQ(mockProcess->lastArgs.at(1), QStringLiteral("src/main.cpp"));
}

TEST_F(DiffCommandTest, OutputsDiffResult)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DiffCommand::registerTo(registry, *mockProcess, output);
    registry.execute(QStringLiteral("diff"), {});

    ASSERT_FALSE(capturedOutput.isEmpty());
    // Should contain diff output (possibly with ANSI codes)
    bool hasDiff = false;
    for (const auto &line : capturedOutput)
    {
        if (line.contains(QStringLiteral("test.txt")) ||
            line.contains(QStringLiteral("old line")) ||
            line.contains(QStringLiteral("new line")))
            hasDiff = true;
    }
    EXPECT_TRUE(hasDiff);
}

TEST_F(DiffCommandTest, HandlesGitError)
{
    mockProcess->simulateError = true;
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DiffCommand::registerTo(registry, *mockProcess, output);
    registry.execute(QStringLiteral("diff"), {});

    ASSERT_FALSE(capturedOutput.isEmpty());
    bool foundError = false;
    for (const auto &line : capturedOutput)
    {
        if (line.contains(QStringLiteral("DIFF_ERROR")) ||
            line.contains(QStringLiteral("not a git repository")))
            foundError = true;
    }
    EXPECT_TRUE(foundError);
}

TEST_F(DiffCommandTest, ReturnsTrueFromHandler)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DiffCommand::registerTo(registry, *mockProcess, output);

    auto info = registry.getCommand(QStringLiteral("diff"));
    ASSERT_NE(info, nullptr);
    EXPECT_TRUE(info->handler({}));
}

// ============================================================
// DiffCommand with empty diff output
// ============================================================

class MockEmptyDiffProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int /*timeoutMs*/ = 30000) override
    {
        callback(0, QString()); // empty output
    }

    void cancel() override {}
};

TEST(DiffCommandEmptyTest, ShowsNoChangesMessage)
{
    MockEmptyDiffProcess emptyProcess;
    CommandRegistry registry;
    QStringList output;

    DiffCommand::registerTo(registry, emptyProcess,
        [&output](const QString &line) { output.append(line); });

    registry.execute(QStringLiteral("diff"), {});

    ASSERT_FALSE(output.isEmpty());
    bool foundNoChanges = false;
    for (const auto &line : output)
    {
        if (line.contains(QStringLiteral("No changes detected")))
            foundNoChanges = true;
    }
    EXPECT_TRUE(foundNoChanges);
}
