#include <gtest/gtest.h>

#include <QJsonArray>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/git_commit_tool.h"

using namespace act::core;

// Mock IProcess for testing git commit — records all calls
class MockCommitProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int timeoutMs = 30000) override
    {
        m_calls.append({command, args});

        if (command == QStringLiteral("git"))
        {
            if (args.contains(QStringLiteral("add")))
                callback(0, QStringLiteral(""));
            else if (args.contains(QStringLiteral("commit")))
            {
                if (m_failCommit)
                    callback(1, QStringLiteral("nothing to commit"));
                else
                    callback(0,
                             QStringLiteral("[main abc1234] test commit"));
            }
            else if (args.contains(QStringLiteral("rev-parse")))
                callback(0, m_commitHash);
            else if (args.contains(QStringLiteral("branch")))
                callback(0, m_branchOutput);
            else
                callback(1, QStringLiteral("unknown command"));
        }
        else
        {
            callback(128, QStringLiteral("command not found"));
        }
    }

    void cancel() override {}

    struct Call
    {
        QString command;
        QStringList args;
    };

    /// Find the last call to git with a specific subcommand argument.
    [[nodiscard]] const Call *findCall(
        const QString &subcommand) const
    {
        for (int i = m_calls.size() - 1; i >= 0; --i)
        {
            if (m_calls[i].command == QStringLiteral("git") &&
                m_calls[i].args.contains(subcommand))
                return &m_calls[i];
        }
        return nullptr;
    }

    QList<Call> m_calls;
    QString m_commitHash = QStringLiteral("abc1234def5678");
    QString m_branchOutput = QStringLiteral("main");
    bool m_failCommit = false;
};

TEST(GitCommitToolTest, NameAndDescription)
{
    MockCommitProcess proc;
    act::harness::GitCommitTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("git_commit"));
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(GitCommitToolTest, PermissionLevelIsWrite)
{
    MockCommitProcess proc;
    act::harness::GitCommitTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Write);
}

TEST(GitCommitToolTest, RequiresMessage)
{
    MockCommitProcess proc;
    act::harness::GitCommitTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

TEST(GitCommitToolTest, SuccessfulCommitReturnsHash)
{
    MockCommitProcess proc;
    act::harness::GitCommitTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("message")] = QStringLiteral("feat: add new feature");
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.contains(QStringLiteral("abc1234")));
}

TEST(GitCommitToolTest, EmptyMessageReturnsError)
{
    MockCommitProcess proc;
    act::harness::GitCommitTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("message")] = QStringLiteral("");
    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
}

TEST(GitCommitToolTest, StageFilesBeforeCommit)
{
    MockCommitProcess proc;
    act::harness::GitCommitTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("message")] = QStringLiteral("feat: add feature");
    params[QStringLiteral("files")] = QJsonArray{
        QStringLiteral("src/foo.cpp"),
        QStringLiteral("src/bar.cpp")};

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);

    // Verify git add was called with the right files
    auto *addCall = proc.findCall(QStringLiteral("add"));
    ASSERT_NE(addCall, nullptr);
    EXPECT_TRUE(addCall->args.contains(QStringLiteral("src/foo.cpp")));
    EXPECT_TRUE(addCall->args.contains(QStringLiteral("src/bar.cpp")));

    // Verify git commit was called
    auto *commitCall = proc.findCall(QStringLiteral("commit"));
    ASSERT_NE(commitCall, nullptr);
    EXPECT_TRUE(commitCall->args.contains(QStringLiteral("commit")));
}

TEST(GitCommitToolTest, AllowEmptyFlag)
{
    MockCommitProcess proc;
    act::harness::GitCommitTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("message")] = QStringLiteral("chore: initial commit");
    params[QStringLiteral("allow_empty")] = true;

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);

    auto *commitCall = proc.findCall(QStringLiteral("commit"));
    ASSERT_NE(commitCall, nullptr);
    EXPECT_TRUE(commitCall->args.contains(QStringLiteral("--allow-empty")));
}

TEST(GitCommitToolTest, ConventionalCommitValidationWarning)
{
    // Non-conventional format should still work (just warns via spdlog)
    MockCommitProcess proc;
    act::harness::GitCommitTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("message")] =
        QStringLiteral("some random message");
    auto result = tool.execute(params);
    // Should still succeed - validation is a warning, not a hard error
    EXPECT_TRUE(result.success);
}
