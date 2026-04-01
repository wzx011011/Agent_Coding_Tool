#include <gtest/gtest.h>

#include <QJsonArray>
#include <QTemporaryDir>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/enter_worktree_tool.h"
#include "harness/tools/exit_worktree_tool.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

// --- Mock IProcess ---

class MockWorktreeProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int timeoutMs = 30000) override
    {
        m_lastCommand = command;
        m_lastArgs = args;

        if (m_failNext)
        {
            callback(128, QStringLiteral("fatal: operation failed"));
            return;
        }

        if (command == QStringLiteral("git"))
        {
            callback(0, m_mockOutput);
        }
        else
        {
            callback(1, QStringLiteral("command not found"));
        }
    }

    void cancel() override {}

    QString m_mockOutput;
    bool m_failNext = false;
    QString m_lastCommand;
    QStringList m_lastArgs;
};

// --- EnterWorktreeTool tests ---

TEST(EnterWorktreeToolTest, NameAndDescription)
{
    MockWorktreeProcess proc;
    EnterWorktreeTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("enter_worktree"));
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(EnterWorktreeToolTest, PermissionLevelIsDestructive)
{
    MockWorktreeProcess proc;
    EnterWorktreeTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Destructive);
}

TEST(EnterWorktreeToolTest, IsNotThreadSafe)
{
    MockWorktreeProcess proc;
    EnterWorktreeTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_FALSE(tool.isThreadSafe());
}

TEST(EnterWorktreeToolTest, SchemaHasOptionalNameAndBranch)
{
    MockWorktreeProcess proc;
    EnterWorktreeTool tool(proc, QStringLiteral("/workspace"));

    auto schema = tool.schema();
    EXPECT_EQ(schema[QStringLiteral("type")].toString(), QStringLiteral("object"));

    auto props = schema[QStringLiteral("properties")].toObject();
    EXPECT_TRUE(props.contains(QStringLiteral("name")));
    EXPECT_TRUE(props.contains(QStringLiteral("branch")));

    // No required fields
    EXPECT_FALSE(schema.contains(QStringLiteral("required")));
}

TEST(EnterWorktreeToolTest, AutoGenerateNameWhenNotProvided)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    MockWorktreeProcess proc;
    proc.m_mockOutput = QStringLiteral("Preparing worktree\n");

    EnterWorktreeTool tool(proc, tmpDir.path());
    auto result = tool.execute(QJsonObject{});

    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(result.output.contains(QStringLiteral("act-worktree-")));
    EXPECT_EQ(proc.m_lastCommand, QStringLiteral("git"));

    // Verify args contain "worktree", "add", "-b"
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("worktree")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("add")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("-b")));
}

TEST(EnterWorktreeToolTest, UsesProvidedNameAndBranch)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    MockWorktreeProcess proc;
    proc.m_mockOutput = QStringLiteral("Preparing worktree\n");

    EnterWorktreeTool tool(proc, tmpDir.path());

    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("my-feature");
    params[QStringLiteral("branch")] = QStringLiteral("feat/my-feature");

    auto result = tool.execute(params);

    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(result.output.contains(QStringLiteral("my-feature")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("feat/my-feature")));
}

TEST(EnterWorktreeToolTest, BranchDefaultsToWorktreeName)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    MockWorktreeProcess proc;
    proc.m_mockOutput = QStringLiteral("Preparing worktree\n");

    EnterWorktreeTool tool(proc, tmpDir.path());

    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("cool-worktree");

    auto result = tool.execute(params);

    EXPECT_TRUE(result.success) << result.error.toStdString();
    // Branch should be same as worktree name
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("cool-worktree")));
}

TEST(EnterWorktreeToolTest, RejectsPathTraversalInName)
{
    MockWorktreeProcess proc;
    EnterWorktreeTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("../etc/passwd");

    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(EnterWorktreeToolTest, RejectsNameWithSlashes)
{
    MockWorktreeProcess proc;
    EnterWorktreeTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("sub/dir");

    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(EnterWorktreeToolTest, RejectsBranchWithSpaces)
{
    MockWorktreeProcess proc;
    EnterWorktreeTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("valid-name");
    params[QStringLiteral("branch")] = QStringLiteral("branch with spaces");

    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(EnterWorktreeToolTest, ReturnsMetadataWithPathAndBranch)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    MockWorktreeProcess proc;
    proc.m_mockOutput = QStringLiteral("Preparing worktree\n");

    EnterWorktreeTool tool(proc, tmpDir.path());

    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("test-wt");

    auto result = tool.execute(params);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.metadata.contains(QStringLiteral("worktree_path")));
    EXPECT_TRUE(result.metadata.contains(QStringLiteral("branch")));
    EXPECT_EQ(result.metadata[QStringLiteral("name")].toString(), QStringLiteral("test-wt"));
}

TEST(EnterWorktreeToolTest, GitFailureReturnsError)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    MockWorktreeProcess proc;
    proc.m_failNext = true;

    EnterWorktreeTool tool(proc, tmpDir.path());

    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("fail-wt");

    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, COMMAND_FAILED);
}

// --- ExitWorktreeTool tests ---

TEST(ExitWorktreeToolTest, NameAndDescription)
{
    MockWorktreeProcess proc;
    ExitWorktreeTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("exit_worktree"));
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(ExitWorktreeToolTest, PermissionLevelIsDestructive)
{
    MockWorktreeProcess proc;
    ExitWorktreeTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Destructive);
}

TEST(ExitWorktreeToolTest, SchemaRequiresAction)
{
    MockWorktreeProcess proc;
    ExitWorktreeTool tool(proc, QStringLiteral("/workspace"));

    auto schema = tool.schema();
    auto required = schema[QStringLiteral("required")].toArray();
    bool hasAction = false;
    for (const auto &item : required)
    {
        if (item.toString() == QStringLiteral("action"))
            hasAction = true;
    }
    EXPECT_TRUE(hasAction);

    auto props = schema[QStringLiteral("properties")].toObject();
    EXPECT_TRUE(props.contains(QStringLiteral("action")));
    EXPECT_TRUE(props.contains(QStringLiteral("worktree_path")));
}

TEST(ExitWorktreeToolTest, MissingActionReturnsError)
{
    MockWorktreeProcess proc;
    ExitWorktreeTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute(QJsonObject{});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(ExitWorktreeToolTest, InvalidActionReturnsError)
{
    MockWorktreeProcess proc;
    ExitWorktreeTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("explode");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(ExitWorktreeToolTest, KeepActionReturnsSuccess)
{
    MockWorktreeProcess proc;
    ExitWorktreeTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("keep");
    params[QStringLiteral("worktree_path")] = QStringLiteral("/workspace/.claude/worktrees/test-wt");

    auto result = tool.execute(params);

    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(result.output.contains(QStringLiteral("kept")));
    // Should NOT call git
    EXPECT_TRUE(proc.m_lastCommand.isEmpty());
}

TEST(ExitWorktreeToolTest, RemoveActionCallsGitWorktreeRemove)
{
    MockWorktreeProcess proc;
    ExitWorktreeTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("remove");
    params[QStringLiteral("worktree_path")] = QStringLiteral("/workspace/.claude/worktrees/test-wt");

    auto result = tool.execute(params);

    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_EQ(proc.m_lastCommand, QStringLiteral("git"));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("worktree")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("remove")));
}

TEST(ExitWorktreeToolTest, RemoveOutsideWorkspaceReturnsError)
{
    MockWorktreeProcess proc;
    ExitWorktreeTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("remove");
    params[QStringLiteral("worktree_path")] = QStringLiteral("/other/place");

    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, OUTSIDE_WORKSPACE);
}

TEST(ExitWorktreeToolTest, RemoveGitFailureReturnsError)
{
    MockWorktreeProcess proc;
    proc.m_failNext = true;
    ExitWorktreeTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("remove");
    params[QStringLiteral("worktree_path")] = QStringLiteral("/workspace/.claude/worktrees/test-wt");

    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, COMMAND_FAILED);
}
