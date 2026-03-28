#include <gtest/gtest.h>

#include <QJsonArray>
#include <QTemporaryDir>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/diff_view_tool.h"

using namespace act::core;

// Mock IProcess for testing diff_view
class MockDiffViewProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int timeoutMs = 30000) override
    {
        m_lastCommand = command;
        m_lastArgs = args;

        if (command == QStringLiteral("git"))
        {
            if (args.contains(QStringLiteral("branch")))
                callback(0, m_branchOutput);
            else if (args.contains(QStringLiteral("diff")))
            {
                // If --stat is present, return stat output
                if (args.contains(QStringLiteral("--stat")))
                    callback(0, m_statOutput);
                else
                    callback(0, m_fullDiffOutput);
            }
            else
                callback(1, QStringLiteral("unknown command"));
        }
        else
        {
            callback(128, QStringLiteral("command not found"));
        }
    }

    void cancel() override {}

    // Test control
    QString m_statOutput =
        QStringLiteral(" src/foo.cpp | 5 +++--\n"
                       " src/bar.cpp | 2 +-\n"
                       " 2 files changed, 4 insertions(+), 3 deletions(-)\n");
    QString m_fullDiffOutput =
        QStringLiteral("diff --git a/src/foo.cpp b/src/foo.cpp\n"
                       "--- a/src/foo.cpp\n"
                       "+++ b/src/foo.cpp\n"
                       "@@ -10,3 +10,5 @@\n"
                       " old line\n"
                       "+new line\n"
                       "+another new\n");
    QString m_branchOutput = QStringLiteral("main");
    QString m_lastCommand;
    QStringList m_lastArgs;
};

// Mock IFileSystem
class MockDiffViewFS : public act::infrastructure::IFileSystem
{
public:
    [[nodiscard]] bool readFile(const QString &path,
                                QString &content) const override
    {
        content = QStringLiteral("dummy");
        return true;
    }

    bool writeFile(const QString &path,
                   const QString &content) override
    {
        return true;
    }

    [[nodiscard]] QStringList listFiles(
        const QString &dir,
        const QString &pattern = QStringLiteral("*")) const override
    {
        return {};
    }

    [[nodiscard]] QString normalizePath(const QString &path) const override
    {
        return path;
    }

    [[nodiscard]] bool exists(const QString &path) const override
    {
        return true;
    }

    bool removeFile(const QString &path) override { return true; }
    bool createDirectory(const QString &path) override { return true; }
};

TEST(DiffViewToolTest, NameAndDescription)
{
    MockDiffViewProcess proc;
    MockDiffViewFS fs;
    act::harness::DiffViewTool tool(proc, fs, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("diff_view"));
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(DiffViewToolTest, PermissionLevelIsRead)
{
    MockDiffViewProcess proc;
    MockDiffViewFS fs;
    act::harness::DiffViewTool tool(proc, fs, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);
}

TEST(DiffViewToolTest, DefaultUnstagedMode)
{
    MockDiffViewProcess proc;
    MockDiffViewFS fs;
    act::harness::DiffViewTool tool(proc, fs, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.contains(QStringLiteral("Change Summary")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("Full Diff")));
}

TEST(DiffViewToolTest, StatOnlyMode)
{
    MockDiffViewProcess proc;
    MockDiffViewFS fs;
    act::harness::DiffViewTool tool(proc, fs, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("stat_only")] = true;
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.contains(QStringLiteral("Change Summary")));
    EXPECT_FALSE(result.output.contains(QStringLiteral("Full Diff")));
}

TEST(DiffViewToolTest, StagedMode)
{
    MockDiffViewProcess proc;
    MockDiffViewFS fs;
    act::harness::DiffViewTool tool(proc, fs, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("mode")] = QStringLiteral("staged");
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("--cached")));
}

TEST(DiffViewToolTest, NoChangesDetected)
{
    MockDiffViewProcess proc;
    proc.m_statOutput = QStringLiteral("");
    proc.m_fullDiffOutput = QStringLiteral("");
    MockDiffViewFS fs;
    act::harness::DiffViewTool tool(proc, fs, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.contains(QStringLiteral("No changes")));
}

TEST(DiffViewToolTest, PathFiltering)
{
    MockDiffViewProcess proc;
    MockDiffViewFS fs;
    act::harness::DiffViewTool tool(proc, fs, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("paths")] = QJsonArray{
        QStringLiteral("src/foo.cpp")};
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("src/foo.cpp")));
}
