#include <gtest/gtest.h>

#include <QDir>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/repo_map_tool.h"

using namespace act::core;

// Mock IFileSystem that simulates a project directory tree
class MockRepoMapFS : public act::infrastructure::IFileSystem
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
        // Normalize dir for comparison
        QString cleanDir = QDir::cleanPath(dir);
        if (cleanDir.endsWith('/'))
            cleanDir.chop(1);

        if (cleanDir == m_baseDir)
            return m_rootEntries;
        if (cleanDir == m_baseDir + QStringLiteral("/src"))
            return m_srcEntries;
        if (cleanDir == m_baseDir + QStringLiteral("/tests"))
            return m_testEntries;
        return {};
    }

    [[nodiscard]] QString normalizePath(const QString &path) const override
    {
        return QDir::cleanPath(path);
    }

    [[nodiscard]] bool exists(const QString &path) const override
    {
        QString clean = QDir::cleanPath(path);
        if (clean.endsWith('/'))
            clean.chop(1);
        // The base directory and anything under it "exists" in this mock
        return clean == m_baseDir || clean.startsWith(m_baseDir + '/');
    }

    bool removeFile(const QString &path) override { return true; }
    bool createDirectory(const QString &path) override { return true; }

    QString m_baseDir = QStringLiteral("/workspace");
    QStringList m_rootEntries = {QStringLiteral("src"),
                                 QStringLiteral("tests"),
                                 QStringLiteral("CMakeLists.txt")};
    QStringList m_srcEntries = {QStringLiteral("main.cpp"),
                                QStringLiteral("utils.cpp")};
    QStringList m_testEntries = {QStringLiteral("test_main.cpp")};
};

// Mock IProcess for git branch info
class MockRepoMapProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int timeoutMs = 30000) override
    {
        if (command == QStringLiteral("git"))
            callback(0, m_branchOutput);
        else
            callback(1, QStringLiteral("unknown command"));
    }

    void cancel() override {}

    QString m_branchOutput = QStringLiteral("main");
};

TEST(RepoMapToolTest, NameAndDescription)
{
    MockRepoMapFS fs;
    MockRepoMapProcess proc;
    act::harness::RepoMapTool tool(fs, proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("repo_map"));
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(RepoMapToolTest, PermissionLevelIsRead)
{
    MockRepoMapFS fs;
    MockRepoMapProcess proc;
    act::harness::RepoMapTool tool(fs, proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);
}

TEST(RepoMapToolTest, IsThreadSafe)
{
    MockRepoMapFS fs;
    MockRepoMapProcess proc;
    act::harness::RepoMapTool tool(fs, proc, QStringLiteral("/workspace"));
    EXPECT_TRUE(tool.isThreadSafe());
}

TEST(RepoMapToolTest, ReturnsProjectStructure)
{
    MockRepoMapFS fs;
    MockRepoMapProcess proc;
    act::harness::RepoMapTool tool(fs, proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.contains(QStringLiteral("Project")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("main")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("Files:")));
}

TEST(RepoMapToolTest, DirectoryNotFound)
{
    MockRepoMapFS fs;
    MockRepoMapProcess proc;
    act::harness::RepoMapTool tool(fs, proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("/nonexistent");
    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::FILE_NOT_FOUND);
}

TEST(RepoMapToolTest, MaxDepthLimitsOutput)
{
    MockRepoMapFS fs;
    MockRepoMapProcess proc;
    act::harness::RepoMapTool tool(fs, proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("max_depth")] = 1;
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    // With depth 1, should show top-level entries including directories
    EXPECT_TRUE(result.output.contains(QStringLiteral("CMakeLists.txt")));
}
