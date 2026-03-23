#include <gtest/gtest.h>

#include <QJsonArray>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/git_status_tool.h"
#include "harness/tools/git_diff_tool.h"

using namespace act::core;

// Mock IProcess for testing git tools
class MockGitProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int timeoutMs = 30000) override
    {
        m_lastCommand = command;
        m_lastArgs = args;

        if (command == QStringLiteral("git") && !m_failGit)
        {
            if (args.contains(QStringLiteral("status")))
                callback(0, m_gitStatusOutput);
            else if (args.contains(QStringLiteral("diff")))
                callback(0, m_gitDiffOutput);
            else
                callback(1, QStringLiteral("unknown git command"));
        }
        else
        {
            callback(128, QStringLiteral("fatal: not a git repository"));
        }
    }

    void cancel() override {}

    // Test control
    QString m_gitStatusOutput;
    QString m_gitDiffOutput;
    bool m_failGit = false;
    QString m_lastCommand;
    QStringList m_lastArgs;
};

TEST(GitStatusToolTest, NameAndDescription)
{
    MockGitProcess proc;
    act::harness::GitStatusTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("git_status"));
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(GitStatusToolTest, PermissionLevelIsRead)
{
    MockGitProcess proc;
    act::harness::GitStatusTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);
}

TEST(GitStatusToolTest, ExecutesGitStatus)
{
    MockGitProcess proc;
    proc.m_gitStatusOutput = QStringLiteral(" M src/file.cpp\nA  new.txt\n");
    act::harness::GitStatusTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_TRUE(result.success);
    EXPECT_EQ(proc.m_lastCommand, QStringLiteral("git"));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("status")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("--porcelain")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("src/file.cpp")));
}

TEST(GitStatusToolTest, NotGitRepoReturnsError)
{
    MockGitProcess proc;
    proc.m_failGit = true;
    act::harness::GitStatusTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::NOT_GIT_REPO);
}

TEST(GitDiffToolTest, NameAndDescription)
{
    MockGitProcess proc;
    act::harness::GitDiffTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("git_diff"));
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(GitDiffToolTest, PermissionLevelIsRead)
{
    MockGitProcess proc;
    act::harness::GitDiffTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);
}

TEST(GitDiffToolTest, ExecutesGitDiff)
{
    MockGitProcess proc;
    proc.m_gitDiffOutput = QStringLiteral("diff --git a/foo b/foo\n--- a/foo\n+++ b/foo\n");
    act::harness::GitDiffTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_TRUE(result.success);
    EXPECT_EQ(proc.m_lastCommand, QStringLiteral("git"));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("diff")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("foo")));
}

TEST(GitDiffToolTest, StagedFlagAddsCached)
{
    MockGitProcess proc;
    proc.m_gitDiffOutput = QStringLiteral("staged changes");
    act::harness::GitDiffTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("staged")] = true;
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("--cached")));
}

TEST(GitDiffToolTest, PathsAppendedAfterDoubleDash)
{
    MockGitProcess proc;
    proc.m_gitDiffOutput = QStringLiteral("file diff");
    act::harness::GitDiffTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("paths")] = QJsonArray{
        QStringLiteral("src/foo.cpp"),
        QStringLiteral("src/bar.cpp")};
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);

    // Find the "--" separator
    int dashIdx = proc.m_lastArgs.indexOf(QStringLiteral("--"));
    ASSERT_GE(dashIdx, 0);
    EXPECT_EQ(proc.m_lastArgs.at(dashIdx + 1), QStringLiteral("src/foo.cpp"));
    EXPECT_EQ(proc.m_lastArgs.at(dashIdx + 2), QStringLiteral("src/bar.cpp"));
}

TEST(GitDiffToolTest, NotGitRepoReturnsError)
{
    MockGitProcess proc;
    proc.m_failGit = true;
    act::harness::GitDiffTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::NOT_GIT_REPO);
}
