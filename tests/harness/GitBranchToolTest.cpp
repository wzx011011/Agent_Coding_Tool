#include <gtest/gtest.h>

#include <QJsonArray>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/git_branch_tool.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

// Mock IProcess for testing git branch operations
class MockBranchProcess : public act::infrastructure::IProcess
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
            callback(0, m_mockOutput);
        }
        else
        {
            callback(128,
                     QStringLiteral("fatal: not a git repository"));
        }
    }

    void cancel() override {}

    QString m_mockOutput;
    bool m_failGit = false;
    QString m_lastCommand;
    QStringList m_lastArgs;
};

TEST(GitBranchToolTest, NameAndDescription)
{
    MockBranchProcess proc;
    GitBranchTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("git_branch"));
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(GitBranchToolTest, PermissionLevelIsExec)
{
    MockBranchProcess proc;
    GitBranchTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Exec);
}

TEST(GitBranchToolTest, ListBranches)
{
    MockBranchProcess proc;
    proc.m_mockOutput =
        QStringLiteral("  feature/a\n* main\n  develop\n");
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("list");
    auto result = tool.execute(params);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(proc.m_lastCommand, QStringLiteral("git"));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("branch")));
}

TEST(GitBranchToolTest, ListCurrentBranch)
{
    MockBranchProcess proc;
    proc.m_mockOutput = QStringLiteral("main\n");
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("list");
    params[QStringLiteral("current")] = true;
    auto result = tool.execute(params);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(
        proc.m_lastArgs.contains(QStringLiteral("--show-current")));
}

TEST(GitBranchToolTest, CreateBranch)
{
    MockBranchProcess proc;
    proc.m_mockOutput = QString();
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("create");
    params[QStringLiteral("name")] = QStringLiteral("feature/new");
    auto result = tool.execute(params);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(proc.m_lastCommand, QStringLiteral("git"));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("branch")));
    EXPECT_TRUE(
        proc.m_lastArgs.contains(QStringLiteral("feature/new")));
    EXPECT_FALSE(proc.m_lastArgs.contains(QStringLiteral("-b")));
}

TEST(GitBranchToolTest, CreateAndSwitchBranch)
{
    MockBranchProcess proc;
    proc.m_mockOutput =
        QStringLiteral("Switched to a new branch 'feature/new'\n");
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("create");
    params[QStringLiteral("name")] = QStringLiteral("feature/new");
    params[QStringLiteral("switch_to")] = true;
    auto result = tool.execute(params);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("checkout")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("-b")));
}

TEST(GitBranchToolTest, DeleteBranch)
{
    MockBranchProcess proc;
    proc.m_mockOutput =
        QStringLiteral("Deleted branch feature/old (was abc1234).\n");
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("delete");
    params[QStringLiteral("name")] = QStringLiteral("feature/old");
    auto result = tool.execute(params);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("-d")));
}

TEST(GitBranchToolTest, ForceDeleteBranch)
{
    MockBranchProcess proc;
    proc.m_mockOutput =
        QStringLiteral("Deleted branch feature/old (was abc1234).\n");
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("delete");
    params[QStringLiteral("name")] = QStringLiteral("feature/old");
    params[QStringLiteral("force")] = true;
    auto result = tool.execute(params);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("-D")));
    EXPECT_FALSE(proc.m_lastArgs.contains(QStringLiteral("-d")));
}

TEST(GitBranchToolTest, SwitchBranch)
{
    MockBranchProcess proc;
    proc.m_mockOutput =
        QStringLiteral("Switched to branch 'develop'\n");
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("switch");
    params[QStringLiteral("name")] = QStringLiteral("develop");
    auto result = tool.execute(params);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("switch")));
}

TEST(GitBranchToolTest, MissingActionParameter)
{
    MockBranchProcess proc;
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(GitBranchToolTest, UnknownAction)
{
    MockBranchProcess proc;
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("merge");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(GitBranchToolTest, MissingNameForCreate)
{
    MockBranchProcess proc;
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("create");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(GitBranchToolTest, MissingNameForSwitch)
{
    MockBranchProcess proc;
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("switch");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(GitBranchToolTest, NotGitRepoReturnsError)
{
    MockBranchProcess proc;
    proc.m_failGit = true;
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("list");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, NOT_GIT_REPO);
}

TEST(GitBranchToolTest, SchemaRequiresAction)
{
    MockBranchProcess proc;
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    auto schema = tool.schema();
    auto required = schema.value(QStringLiteral("required")).toArray();
    bool hasAction = false;
    for (const auto &item : required)
    {
        if (item.toString() == QStringLiteral("action"))
            hasAction = true;
    }
    EXPECT_TRUE(hasAction);
}

TEST(GitBranchToolTest, DefaultListAction)
{
    MockBranchProcess proc;
    proc.m_mockOutput = QStringLiteral("* main\n  develop\n");
    GitBranchTool tool(proc, QStringLiteral("/workspace"));

    // No action specified — should return error (action is required)
    QJsonObject params;
    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}
