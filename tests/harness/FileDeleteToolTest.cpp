#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "core/error_codes.h"
#include "harness/tools/file_delete_tool.h"
#include "mock_file_system.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

class FileDeleteToolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tmpDir.emplace();
        fs = std::make_unique<MockFileSystem>(tmpDir->path());
        tool = std::make_unique<FileDeleteTool>(*fs, tmpDir->path());
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
    std::unique_ptr<FileDeleteTool> tool;
    QString root;
};

TEST_F(FileDeleteToolTest, NameAndMetadata)
{
    EXPECT_EQ(tool->name(), QStringLiteral("file_delete"));
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Destructive);
    EXPECT_FALSE(tool->isThreadSafe());
}

TEST_F(FileDeleteToolTest, DeleteExistingFile)
{
    const QString filePath =
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("to_delete.txt"));
    fs->writeFile(filePath, QStringLiteral("content"));

    ASSERT_TRUE(fs->exists(filePath));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("to_delete.txt");

    auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_FALSE(fs->exists(filePath));
    EXPECT_EQ(result.metadata.value(QStringLiteral("path")).toString(), filePath);
}

TEST_F(FileDeleteToolTest, FileNotFound)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("nonexistent.txt");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, FILE_NOT_FOUND);
}

TEST_F(FileDeleteToolTest, OutsideWorkspace)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("/etc/passwd");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, OUTSIDE_WORKSPACE);
}

TEST_F(FileDeleteToolTest, MissingPathParameter)
{
    QJsonObject params;

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(FileDeleteToolTest, InvalidPathType)
{
    QJsonObject params;
    params[QStringLiteral("path")] = 42;

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(FileDeleteToolTest, AbsolutePathWithinWorkspace)
{
    const QString filePath =
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("abs_delete.txt"));
    fs->writeFile(filePath, QStringLiteral("data"));

    QJsonObject params;
    params[QStringLiteral("path")] = filePath;

    auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_FALSE(fs->exists(filePath));
}
