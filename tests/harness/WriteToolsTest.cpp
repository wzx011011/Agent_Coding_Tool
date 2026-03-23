#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "core/error_codes.h"
#include "harness/permission_manager.h"
#include "harness/tools/file_edit_tool.h"
#include "harness/tools/file_read_tool.h"
#include "harness/tools/file_write_tool.h"
#include "harness/tool_registry.h"
#include "mock_file_system.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

// ============================================================
// PermissionManager Tests
// ============================================================

TEST(PermissionManagerTest, ReadIsAutoApprovedByDefault)
{
    PermissionManager pm;
    EXPECT_TRUE(pm.isAutoApproved(PermissionLevel::Read));
}

TEST(PermissionManagerTest, WriteNotAutoApprovedByDefault)
{
    PermissionManager pm;
    EXPECT_FALSE(pm.isAutoApproved(PermissionLevel::Write));
    EXPECT_FALSE(pm.isAutoApproved(PermissionLevel::Exec));
    EXPECT_FALSE(pm.isAutoApproved(PermissionLevel::Destructive));
}

TEST(PermissionManagerTest, CanSetAutoApprove)
{
    PermissionManager pm;
    pm.setAutoApproved(PermissionLevel::Write, true);
    EXPECT_TRUE(pm.isAutoApproved(PermissionLevel::Write));
}

TEST(PermissionManagerTest, DenyList)
{
    PermissionManager pm;
    pm.addToDenyList(QStringLiteral("dangerous_tool"));
    EXPECT_TRUE(pm.isDenied(QStringLiteral("dangerous_tool")));
    EXPECT_FALSE(pm.isDenied(QStringLiteral("safe_tool")));
}

TEST(PermissionManagerTest, AutoApprovedReadReturnsApproved)
{
    PermissionManager pm;
    auto result = pm.checkPermission(
        PermissionLevel::Read, QStringLiteral("file_read"), QStringLiteral("test"));
    EXPECT_EQ(result, PermissionManager::Decision::Approved);
}

TEST(PermissionManagerTest, NonAutoApprovedReturnsDenied)
{
    PermissionManager pm;
    auto result = pm.checkPermission(
        PermissionLevel::Write, QStringLiteral("file_write"), QStringLiteral("test"));
    EXPECT_EQ(result, PermissionManager::Decision::Denied);
}

TEST(PermissionManagerTest, DenyListBlocksEvenAutoApproved)
{
    PermissionManager pm;
    pm.setAutoApproved(PermissionLevel::Write, true);
    pm.addToDenyList(QStringLiteral("blocked_tool"));
    auto result = pm.checkPermission(
        PermissionLevel::Write, QStringLiteral("blocked_tool"), QStringLiteral("test"));
    EXPECT_EQ(result, PermissionManager::Decision::Denied);
}

// ============================================================
// FileWriteTool Tests
// ============================================================

class FileWriteToolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tmpDir.emplace();
        fs = std::make_unique<MockFileSystem>(tmpDir->path());
        tool = std::make_unique<FileWriteTool>(*fs, tmpDir->path());
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
    std::unique_ptr<FileWriteTool> tool;
    QString root;
};

TEST_F(FileWriteToolTest, NameAndMetadata)
{
    EXPECT_EQ(tool->name(), QStringLiteral("file_write"));
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Write);
    EXPECT_FALSE(tool->isThreadSafe());
}

TEST_F(FileWriteToolTest, WriteNewFile)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("new_file.txt");
    params[QStringLiteral("content")] = QStringLiteral("Hello World");

    auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    // Verify file was written
    QString content;
    ASSERT_TRUE(fs->readFile(
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("new_file.txt")),
        content));
    EXPECT_EQ(content, QStringLiteral("Hello World"));
}

TEST_F(FileWriteToolTest, OverwriteExistingFile)
{
    // Create initial file
    fs->writeFile(
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("existing.txt")),
        QStringLiteral("Original content"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("existing.txt");
    params[QStringLiteral("content")] = QStringLiteral("Updated content");

    auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    QString content;
    fs->readFile(
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("existing.txt")),
        content);
    EXPECT_EQ(content, QStringLiteral("Updated content"));
}

TEST_F(FileWriteToolTest, RejectOutsideWorkspace)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("/etc/passwd");
    params[QStringLiteral("content")] = QStringLiteral("hack");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::OUTSIDE_WORKSPACE);
}

TEST_F(FileWriteToolTest, MissingPath)
{
    QJsonObject params;
    params[QStringLiteral("content")] = QStringLiteral("test");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

TEST_F(FileWriteToolTest, MissingContent)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.txt");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

TEST_F(FileWriteToolTest, InvalidPathType)
{
    QJsonObject params;
    params[QStringLiteral("path")] = 42;
    params[QStringLiteral("content")] = QStringLiteral("test");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

// ============================================================
// FileEditTool Tests
// ============================================================

class FileEditToolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tmpDir.emplace();
        fs = std::make_unique<MockFileSystem>(tmpDir->path());
        tool = std::make_unique<FileEditTool>(*fs, tmpDir->path());
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
    std::unique_ptr<FileEditTool> tool;
    QString root;
};

TEST_F(FileEditToolTest, NameAndMetadata)
{
    EXPECT_EQ(tool->name(), QStringLiteral("file_edit"));
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Write);
    EXPECT_FALSE(tool->isThreadSafe());
}

TEST_F(FileEditToolTest, EditFile)
{
    // Create file with content
    const QString filePath =
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("editme.txt"));
    fs->writeFile(filePath, QStringLiteral("Hello World"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("editme.txt");
    params[QStringLiteral("old_string")] = QStringLiteral("World");
    params[QStringLiteral("new_string")] = QStringLiteral("Universe");

    auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    QString content;
    fs->readFile(filePath, content);
    EXPECT_EQ(content, QStringLiteral("Hello Universe"));
}

TEST_F(FileEditToolTest, StringNotFound)
{
    const QString filePath =
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("editme.txt"));
    fs->writeFile(filePath, QStringLiteral("Hello World"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("editme.txt");
    params[QStringLiteral("old_string")] = QStringLiteral("NotFound");
    params[QStringLiteral("new_string")] = QStringLiteral("Replaced");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::STRING_NOT_FOUND);
}

TEST_F(FileEditToolTest, AmbiguousMatch)
{
    const QString filePath =
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("editme.txt"));
    fs->writeFile(filePath, QStringLiteral("aaa aaa aaa"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("editme.txt");
    params[QStringLiteral("old_string")] = QStringLiteral("aaa");
    params[QStringLiteral("new_string")] = QStringLiteral("bbb");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::AMBIGUOUS_MATCH);
}

TEST_F(FileEditToolTest, FileNotFound)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("nonexistent.txt");
    params[QStringLiteral("old_string")] = QStringLiteral("old");
    params[QStringLiteral("new_string")] = QStringLiteral("new");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::FILE_NOT_FOUND);
}

TEST_F(FileEditToolTest, OutsideWorkspace)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("/etc/passwd");
    params[QStringLiteral("old_string")] = QStringLiteral("old");
    params[QStringLiteral("new_string")] = QStringLiteral("new");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::OUTSIDE_WORKSPACE);
}

TEST_F(FileEditToolTest, MissingParameters)
{
    QJsonObject params;
    // Missing old_string and new_string
    params[QStringLiteral("path")] = QStringLiteral("editme.txt");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

// ============================================================
// Integration: register write tools in ToolRegistry
// ============================================================

TEST(WriteToolsIntegration, RegisterAndExecuteWriteTools)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    const QString root = QDir::cleanPath(tmpDir.path());

    MockFileSystem fs(root);
    ToolRegistry registry;

    registry.registerTool(std::make_unique<FileReadTool>(fs, root));
    registry.registerTool(std::make_unique<FileWriteTool>(fs, root));
    registry.registerTool(std::make_unique<FileEditTool>(fs, root));

    // Write a file
    QJsonObject writeParams;
    writeParams[QStringLiteral("path")] = QStringLiteral("test.txt");
    writeParams[QStringLiteral("content")] = QStringLiteral("line one\nline two\nline three");
    auto writeResult =
        registry.execute(QStringLiteral("file_write"), writeParams);
    ASSERT_TRUE(writeResult.success) << writeResult.error.toStdString();

    // Read it back
    QJsonObject readParams;
    readParams[QStringLiteral("path")] = QStringLiteral("test.txt");
    auto readResult = registry.execute(QStringLiteral("file_read"), readParams);
    ASSERT_TRUE(readResult.success) << readResult.error.toStdString();
    EXPECT_TRUE(readResult.output.contains(QStringLiteral("line one")));
    EXPECT_TRUE(readResult.output.contains(QStringLiteral("line three")));

    // Edit it
    QJsonObject editParams;
    editParams[QStringLiteral("path")] = QStringLiteral("test.txt");
    editParams[QStringLiteral("old_string")] = QStringLiteral("line two");
    editParams[QStringLiteral("new_string")] = QStringLiteral("edited line");
    auto editResult =
        registry.execute(QStringLiteral("file_edit"), editParams);
    ASSERT_TRUE(editResult.success) << editResult.error.toStdString();

    // Verify edit
    readResult = registry.execute(QStringLiteral("file_read"), readParams);
    ASSERT_TRUE(readResult.success);
    EXPECT_TRUE(readResult.output.contains(QStringLiteral("edited line")));
    EXPECT_FALSE(readResult.output.contains(QStringLiteral("line two")));
}
