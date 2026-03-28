#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "core/error_codes.h"
#include "harness/tools/directory_tool.h"
#include "mock_file_system.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

class DirectoryToolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tmpDir.emplace();
        fs = std::make_unique<MockFileSystem>(tmpDir->path());
        tool = std::make_unique<DirectoryTool>(*fs, tmpDir->path());
        root = QDir::cleanPath(tmpDir->path());
    }

    void TearDown() override
    {
        tool.reset();
        fs.reset();
        tmpDir.reset();
    }

    std::optional<QTemporaryDir> tmpDir;
    std::unique_ptr<MockFileSystem> fs;
    std::unique_ptr<DirectoryTool> tool;
    QString root;
};

TEST_F(DirectoryToolTest, NameAndMetadata)
{
    EXPECT_EQ(tool->name(), QStringLiteral("directory_create"));
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Write);
    EXPECT_FALSE(tool->isThreadSafe());
}

TEST_F(DirectoryToolTest, CreateSingleDirectory)
{
    const QString dirPath =
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("new_dir"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("new_dir");

    auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(QDir(dirPath).exists());
    EXPECT_FALSE(result.metadata.value(QStringLiteral("already_existed")).toBool());
}

TEST_F(DirectoryToolTest, CreateNestedDirectories)
{
    const QString dirPath =
        QDir::cleanPath(root + QLatin1Char('/') +
                        QStringLiteral("a/b/c/deep"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("a/b/c/deep");

    auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(QDir(dirPath).exists());
}

TEST_F(DirectoryToolTest, AlreadyExists)
{
    const QString dirPath =
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("existing"));
    QDir().mkpath(dirPath);

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("existing");

    auto result = tool->execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.metadata.value(QStringLiteral("already_existed")).toBool());
    EXPECT_TRUE(result.output.contains(QStringLiteral("already exists")));
}

TEST_F(DirectoryToolTest, OutsideWorkspace)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("/tmp/evil_dir");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, OUTSIDE_WORKSPACE);
}

TEST_F(DirectoryToolTest, MissingPathParameter)
{
    QJsonObject params;

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(DirectoryToolTest, EmptyPathParameter)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(DirectoryToolTest, InvalidPathType)
{
    QJsonObject params;
    params[QStringLiteral("path")] = 123;

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(DirectoryToolTest, WorkspaceRootIsAllowed)
{
    // Creating the workspace root itself should be allowed (already exists)
    QJsonObject params;
    params[QStringLiteral("path")] = root;

    auto result = tool->execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.metadata.value(QStringLiteral("already_existed")).toBool());
}
