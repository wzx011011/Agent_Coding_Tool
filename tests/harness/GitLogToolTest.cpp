#include <gtest/gtest.h>

#include <QJsonArray>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/git_log_tool.h"

using namespace act::core;

// Mock IProcess for testing GitLogTool
class MockGitLogProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int timeoutMs = 30000) override
    {
        m_lastCommand = command;
        m_lastArgs = args;

        if (command == QStringLiteral("git") && args.contains(QStringLiteral("log"))
            && !m_failGit)
        {
            callback(0, m_gitLogOutput);
        }
        else
        {
            callback(128, QStringLiteral("fatal: not a git repository"));
        }
    }

    void cancel() override {}

    // Test control
    QString m_gitLogOutput;
    bool m_failGit = false;
    QString m_lastCommand;
    QStringList m_lastArgs;
};

TEST(GitLogToolTest, NameAndDescription)
{
    MockGitLogProcess proc;
    act::harness::GitLogTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("git_log"));
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(GitLogToolTest, PermissionLevelIsRead)
{
    MockGitLogProcess proc;
    act::harness::GitLogTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);
}

TEST(GitLogToolTest, ExecutesGitLog)
{
    MockGitLogProcess proc;
    proc.m_gitLogOutput =
        QStringLiteral("a1b2c3d feat: add new feature\n"
                       "e4f5g6h fix: resolve crash\n");
    act::harness::GitLogTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_TRUE(result.success);
    EXPECT_EQ(proc.m_lastCommand, QStringLiteral("git"));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("log")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("--oneline")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("a1b2c3d")));
}

TEST(GitLogToolTest, CountParameter)
{
    MockGitLogProcess proc;
    proc.m_gitLogOutput = QStringLiteral("a1b2c3d single commit\n");
    act::harness::GitLogTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("count")] = 5;
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("-n5")));
}

TEST(GitLogToolTest, AuthorFilter)
{
    MockGitLogProcess proc;
    proc.m_gitLogOutput = QStringLiteral("a1b2c3d author commit\n");
    act::harness::GitLogTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("author")] = QStringLiteral("alice");
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(
        proc.m_lastArgs.contains(QStringLiteral("--author=alice")));
}

TEST(GitLogToolTest, GrepFilter)
{
    MockGitLogProcess proc;
    proc.m_gitLogOutput = QStringLiteral("a1b2c3d matching commit\n");
    act::harness::GitLogTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("grep")] = QStringLiteral("bugfix");
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(
        proc.m_lastArgs.contains(QStringLiteral("--grep=bugfix")));
}

TEST(GitLogToolTest, SinceFilter)
{
    MockGitLogProcess proc;
    proc.m_gitLogOutput = QStringLiteral("a1b2c3d recent commit\n");
    act::harness::GitLogTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("since")] = QStringLiteral("2024-01-01");
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(
        proc.m_lastArgs.contains(QStringLiteral("--since=2024-01-01")));
}

TEST(GitLogToolTest, MultipleFilters)
{
    MockGitLogProcess proc;
    proc.m_gitLogOutput = QStringLiteral("a1b2c3d filtered commit\n");
    act::harness::GitLogTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("count")] = 10;
    params[QStringLiteral("author")] = QStringLiteral("bob");
    params[QStringLiteral("grep")] = QStringLiteral("feature");
    params[QStringLiteral("since")] = QStringLiteral("2024-06-01");
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);

    // Verify all filters are present in the args
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("log")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("--oneline")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("-n10")));
    EXPECT_TRUE(
        proc.m_lastArgs.contains(QStringLiteral("--author=bob")));
    EXPECT_TRUE(
        proc.m_lastArgs.contains(QStringLiteral("--grep=feature")));
    EXPECT_TRUE(
        proc.m_lastArgs.contains(QStringLiteral("--since=2024-06-01")));
}

TEST(GitLogToolTest, NotGitRepoReturnsError)
{
    MockGitLogProcess proc;
    proc.m_failGit = true;
    act::harness::GitLogTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::NOT_GIT_REPO);
}

TEST(GitLogToolTest, SchemaHasNoRequiredFields)
{
    MockGitLogProcess proc;
    act::harness::GitLogTool tool(proc, QStringLiteral("/workspace"));

    auto schema = tool.schema();
    auto required = schema.value(QStringLiteral("required")).toArray();
    EXPECT_TRUE(required.isEmpty());
}
