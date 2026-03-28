#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>

#include <stdexcept>

#include <memory>

#include "core/error_codes.h"
#include "harness/tool_registry.h"
#include "harness/tools/file_read_tool.h"
#include "harness/tools/grep_tool.h"
#include "harness/tools/glob_tool.h"
#include "infrastructure/interfaces.h"

using namespace act::harness;
using namespace act::core;

namespace
{

// Mock IFileSystem that delegates to real QFile operations with a fixed workspace root
class MockFileSystem : public act::infrastructure::IFileSystem
{
public:
    explicit MockFileSystem(QString workspaceRoot)
        : m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
    {
    }

    [[nodiscard]] bool readFile(const QString &path,
                                QString &content) const override
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return false;
        }
        content = QString::fromUtf8(file.readAll());
        return true;
    }

    bool writeFile(const QString &path,
                   const QString &content) override
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            return false;
        }
        file.write(content.toUtf8());
        return true;
    }

    [[nodiscard]] QStringList listFiles(const QString &dir,
                                        const QString &pattern) const override
    {
        QDir d(dir);
        return d.entryList({pattern}, QDir::Files);
    }

    [[nodiscard]] QString normalizePath(const QString &path) const override
    {
        if (QDir::isRelativePath(path))
        {
            return QDir::cleanPath(m_workspaceRoot + QLatin1Char('/') + path);
        }
        return QDir::cleanPath(path);
    }

    [[nodiscard]] bool exists(const QString &path) const override
    {
        return QFile::exists(path) || QDir(path).exists();
    }

    bool removeFile(const QString &path) override
    {
        return QFile::remove(path);
    }

    bool createDirectory(const QString &path) override
    {
        return QDir().mkpath(path);
    }

private:
    QString m_workspaceRoot;
};

// RAII helper to create a temp directory with files for testing
struct TempWorkspace
{
    QTemporaryDir dir;
    QString root;
    MockFileSystem *fs = nullptr;

    TempWorkspace()
        : dir()
        , root(QDir::cleanPath(dir.path()))
    {
        if (!dir.isValid())
            throw std::runtime_error("Failed to create temporary directory");
        fs = new MockFileSystem(root);
    }

    ~TempWorkspace()
    {
        delete fs;
    }

    // Disable copy
    TempWorkspace(const TempWorkspace &) = delete;
    TempWorkspace &operator=(const TempWorkspace &) = delete;

    QString makeFile(const QString &relativePath, const QString &content) const
    {
        const QString fullPath = QDir::cleanPath(root + QLatin1Char('/') + relativePath);
        QDir().mkpath(QFileInfo(fullPath).absolutePath());

        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            throw std::runtime_error(
                QStringLiteral("Cannot create file: ").toStdString() +
                fullPath.toStdString());
        file.write(content.toUtf8());
        file.close();
        return fullPath;
    }

    QString makeSubDir(const QString &relativePath) const
    {
        const QString fullPath = QDir::cleanPath(root + QLatin1Char('/') + relativePath);
        QDir().mkpath(fullPath);
        return fullPath;
    }
};

} // anonymous namespace

// ============================================================
// FileReadTool Tests
// ============================================================

class FileReadToolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ws = std::make_unique<TempWorkspace>();
        tool = std::make_unique<FileReadTool>(*ws->fs, ws->root);
    }

    void TearDown() override
    {
        tool.reset();
        ws.reset();
    }

    std::unique_ptr<TempWorkspace> ws;
    std::unique_ptr<FileReadTool> tool;
};

TEST_F(FileReadToolTest, NameAndMetadata)
{
    EXPECT_EQ(tool->name(), QStringLiteral("file_read"));
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Read);
    EXPECT_TRUE(tool->isThreadSafe());
    EXPECT_FALSE(tool->description().isEmpty());
}

TEST_F(FileReadToolTest, SchemaRequiresPath)
{
    auto schema = tool->schema();
    auto required = schema.value(QStringLiteral("required")).toArray();
    EXPECT_EQ(required.size(), 1);
    EXPECT_EQ(required.at(0).toString(), QStringLiteral("path"));
}

TEST_F(FileReadToolTest, ReadExistingFile)
{
    ws->makeFile(QStringLiteral("hello.txt"), QStringLiteral("Hello, World!"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("hello.txt");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success) << qPrintable(result.error);
    EXPECT_EQ(result.output, QStringLiteral("Hello, World!"));
    EXPECT_EQ(result.metadata.value(QStringLiteral("path")).toString(),
              QDir::cleanPath(ws->root + QLatin1Char('/') + QStringLiteral("hello.txt")));
}

TEST_F(FileReadToolTest, ReadNestedFile)
{
    ws->makeFile(QStringLiteral("sub/deep/file.txt"), QStringLiteral("nested content"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("sub/deep/file.txt");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success) << qPrintable(result.error);
    EXPECT_EQ(result.output, QStringLiteral("nested content"));
}

TEST_F(FileReadToolTest, ReadNonExistentFile)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("nonexistent.txt");
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::FILE_NOT_FOUND);
}

TEST_F(FileReadToolTest, ReadOutsideWorkspace)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("/etc/passwd");
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::OUTSIDE_WORKSPACE);
}

TEST_F(FileReadToolTest, MissingPathParameter)
{
    QJsonObject params; // no "path" key
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

TEST_F(FileReadToolTest, PathMustBeString)
{
    QJsonObject params;
    params[QStringLiteral("path")] = 42;
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

TEST_F(FileReadToolTest, DetectBinaryFile)
{
    // Create a file with a null byte in its content
    const QString binPath = ws->makeFile(QStringLiteral("binary.dat"), QString());
    QFile file(binPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    // Write some text followed by a null byte
    file.write("text\x00binary", 11);
    file.close();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("binary.dat");
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::BINARY_FILE);
}

TEST_F(FileReadToolTest, RejectWorkspaceRootItself)
{
    QJsonObject params;
    params[QStringLiteral("path")] = ws->root;
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::OUTSIDE_WORKSPACE);
}

TEST_F(FileReadToolTest, RejectPathTraversalOutsideWorkspace)
{
    QJsonObject params;
    // Try to escape workspace with ../
    params[QStringLiteral("path")] = QStringLiteral("../outside.txt");
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::OUTSIDE_WORKSPACE);
}

// ============================================================
// GrepTool Tests
// ============================================================

class GrepToolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ws = std::make_unique<TempWorkspace>();
        tool = std::make_unique<GrepTool>(*ws->fs, ws->root);
    }

    void TearDown() override
    {
        tool.reset();
        ws.reset();
    }

    std::unique_ptr<TempWorkspace> ws;
    std::unique_ptr<GrepTool> tool;
};

TEST_F(GrepToolTest, NameAndMetadata)
{
    EXPECT_EQ(tool->name(), QStringLiteral("grep"));
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Read);
    EXPECT_TRUE(tool->isThreadSafe());
    EXPECT_FALSE(tool->description().isEmpty());
}

TEST_F(GrepToolTest, SchemaRequiresPattern)
{
    auto schema = tool->schema();
    auto required = schema.value(QStringLiteral("required")).toArray();
    EXPECT_EQ(required.size(), 1);
    EXPECT_EQ(required.at(0).toString(), QStringLiteral("pattern"));
}

TEST_F(GrepToolTest, GrepSingleFileWithMatches)
{
    ws->makeFile(QStringLiteral("code.cpp"), QStringLiteral(
        "line one\n"
        "hello world\n"
        "line three\n"
        "hello again\n"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("hello");
    params[QStringLiteral("path")] = QStringLiteral("code.cpp");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success) << qPrintable(result.error);
    // Should contain two matching lines
    EXPECT_TRUE(result.output.contains(QStringLiteral("hello world")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("hello again")));
}

TEST_F(GrepToolTest, GrepSingleFileNoMatches)
{
    ws->makeFile(QStringLiteral("code.cpp"), QStringLiteral(
        "alpha\nbeta\ngamma\n"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("delta");
    params[QStringLiteral("path")] = QStringLiteral("code.cpp");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.output, QStringLiteral("(no matches found)"));
}

TEST_F(GrepToolTest, GrepDirectory)
{
    ws->makeFile(QStringLiteral("dir/a.txt"), QStringLiteral("foo bar\nbaz\n"));
    ws->makeFile(QStringLiteral("dir/b.txt"), QStringLiteral("hello\nworld\n"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("foo");
    params[QStringLiteral("path")] = QStringLiteral("dir");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success) << qPrintable(result.error);
    EXPECT_TRUE(result.output.contains(QStringLiteral("foo bar")));
}

TEST_F(GrepToolTest, GrepDirectoryWithGlobFilter)
{
    ws->makeFile(QStringLiteral("dir/test.cpp"), QStringLiteral("match_here\n"));
    ws->makeFile(QStringLiteral("dir/test.h"), QStringLiteral("nope\n"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("match_here");
    params[QStringLiteral("path")] = QStringLiteral("dir");
    params[QStringLiteral("glob")] = QStringLiteral("*.cpp");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success) << qPrintable(result.error);
    EXPECT_TRUE(result.output.contains(QStringLiteral("match_here")));
}

TEST_F(GrepToolTest, GrepDefaultsToWorkspaceRoot)
{
    ws->makeFile(QStringLiteral("rootfile.txt"), QStringLiteral("unique_xyz\n"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("unique_xyz");
    // no "path" specified -- should search workspace root
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success) << qPrintable(result.error);
    EXPECT_TRUE(result.output.contains(QStringLiteral("unique_xyz")));
}

TEST_F(GrepToolTest, GrepOutsideWorkspace)
{
    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("anything");
    params[QStringLiteral("path")] = QStringLiteral("/etc");
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::OUTSIDE_WORKSPACE);
}

TEST_F(GrepToolTest, GrepInvalidRegex)
{
    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("[invalid");
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PATTERN);
}

TEST_F(GrepToolTest, GrepMissingPattern)
{
    QJsonObject params; // no "pattern"
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

TEST_F(GrepToolTest, GrepPathNotFound)
{
    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("test");
    params[QStringLiteral("path")] = QStringLiteral("nonexistent_dir");
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::FILE_NOT_FOUND);
}

TEST_F(GrepToolTest, GrepLineNumberIncluded)
{
    ws->makeFile(QStringLiteral("lines.txt"), QStringLiteral(
        "first\nsecond\nthird\n"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("second");
    params[QStringLiteral("path")] = QStringLiteral("lines.txt");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success) << qPrintable(result.error);
    // Should include line number 2
    EXPECT_TRUE(result.output.contains(QStringLiteral(":2:")));
}

// ============================================================
// GlobTool Tests
// ============================================================

class GlobToolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ws = std::make_unique<TempWorkspace>();
        tool = std::make_unique<GlobTool>(*ws->fs, ws->root);
    }

    void TearDown() override
    {
        tool.reset();
        ws.reset();
    }

    std::unique_ptr<TempWorkspace> ws;
    std::unique_ptr<GlobTool> tool;
};

TEST_F(GlobToolTest, NameAndMetadata)
{
    EXPECT_EQ(tool->name(), QStringLiteral("glob"));
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Read);
    EXPECT_TRUE(tool->isThreadSafe());
    EXPECT_FALSE(tool->description().isEmpty());
}

TEST_F(GlobToolTest, SchemaRequiresPattern)
{
    auto schema = tool->schema();
    auto required = schema.value(QStringLiteral("required")).toArray();
    EXPECT_EQ(required.size(), 1);
    EXPECT_EQ(required.at(0).toString(), QStringLiteral("pattern"));
}

TEST_F(GlobToolTest, GlobMatchesFiles)
{
    ws->makeFile(QStringLiteral("a.txt"), QStringLiteral("a"));
    ws->makeFile(QStringLiteral("b.txt"), QStringLiteral("b"));
    ws->makeFile(QStringLiteral("c.cpp"), QStringLiteral("c"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("*.txt");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success) << qPrintable(result.error);
    EXPECT_TRUE(result.output.contains(QStringLiteral("a.txt")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("b.txt")));
    EXPECT_FALSE(result.output.contains(QStringLiteral("c.cpp")));
}

TEST_F(GlobToolTest, GlobRecursiveSearch)
{
    ws->makeFile(QStringLiteral("src/main.cpp"), QStringLiteral("main"));
    ws->makeFile(QStringLiteral("src/util/util.cpp"), QStringLiteral("util"));
    ws->makeFile(QStringLiteral("docs/readme.md"), QStringLiteral("readme"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("*.cpp");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success) << qPrintable(result.error);
    EXPECT_TRUE(result.output.contains(QStringLiteral("main.cpp")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("util.cpp")));
    EXPECT_FALSE(result.output.contains(QStringLiteral("readme.md")));
}

TEST_F(GlobToolTest, GlobNoMatches)
{
    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("*.xyz");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.output, QStringLiteral("(no files matched)"));
    EXPECT_EQ(result.metadata.value(QStringLiteral("matchCount")).toInt(), 0);
}

TEST_F(GlobToolTest, GlobWithSubdirectoryPath)
{
    ws->makeFile(QStringLiteral("sub/one.txt"), QStringLiteral("one"));
    ws->makeFile(QStringLiteral("other/two.txt"), QStringLiteral("two"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("*.txt");
    params[QStringLiteral("path")] = QStringLiteral("sub");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success) << qPrintable(result.error);
    EXPECT_TRUE(result.output.contains(QStringLiteral("one.txt")));
    EXPECT_FALSE(result.output.contains(QStringLiteral("two.txt")));
}

TEST_F(GlobToolTest, GlobOutsideWorkspace)
{
    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("*");
    params[QStringLiteral("path")] = QStringLiteral("/etc");
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::OUTSIDE_WORKSPACE);
}

TEST_F(GlobToolTest, GlobDirectoryNotFound)
{
    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("*");
    params[QStringLiteral("path")] = QStringLiteral("nonexistent_dir");
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::FILE_NOT_FOUND);
}

TEST_F(GlobToolTest, GlobMissingPattern)
{
    QJsonObject params; // no "pattern"
    auto result = tool->execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

TEST_F(GlobToolTest, GlobMatchCountInMetadata)
{
    ws->makeFile(QStringLiteral("a.txt"), QStringLiteral("a"));
    ws->makeFile(QStringLiteral("b.txt"), QStringLiteral("b"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("*.txt");
    auto result = tool->execute(params);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.metadata.value(QStringLiteral("matchCount")).toInt(), 2);
}

// ============================================================
// Integration: register all three tools in ToolRegistry
// ============================================================

TEST(ReadOnlyToolsIntegration, RegisterAndExecuteAllTools)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    const QString root = QDir::cleanPath(tmpDir.path());

    // Create test file
    const QString filePath = QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("test.txt"));
    QFile file(filePath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("hello world\nfoo bar\n");
    file.close();

    MockFileSystem fs(root);
    ToolRegistry registry;

    registry.registerTool(std::make_unique<FileReadTool>(fs, root));
    registry.registerTool(std::make_unique<GrepTool>(fs, root));
    registry.registerTool(std::make_unique<GlobTool>(fs, root));

    EXPECT_EQ(registry.size(), 3u);

    // Execute file_read
    QJsonObject readParams;
    readParams[QStringLiteral("path")] = QStringLiteral("test.txt");
    auto readResult = registry.execute(QStringLiteral("file_read"), readParams);
    ASSERT_TRUE(readResult.success) << qPrintable(readResult.error);
    EXPECT_TRUE(readResult.output.contains(QStringLiteral("hello world")));

    // Execute grep
    QJsonObject grepParams;
    grepParams[QStringLiteral("pattern")] = QStringLiteral("foo");
    grepParams[QStringLiteral("path")] = filePath;
    auto grepResult = registry.execute(QStringLiteral("grep"), grepParams);
    ASSERT_TRUE(grepResult.success) << qPrintable(grepResult.error);
    EXPECT_TRUE(grepResult.output.contains(QStringLiteral("foo bar")));

    // Execute glob
    QJsonObject globParams;
    globParams[QStringLiteral("pattern")] = QStringLiteral("*.txt");
    auto globResult = registry.execute(QStringLiteral("glob"), globParams);
    ASSERT_TRUE(globResult.success) << qPrintable(globResult.error);
    EXPECT_TRUE(globResult.output.contains(QStringLiteral("test.txt")));

    // Execute unknown tool
    auto unknownResult = registry.execute(QStringLiteral("nonexistent"), {});
    EXPECT_FALSE(unknownResult.success);
    EXPECT_EQ(unknownResult.errorCode, errors::TOOL_NOT_FOUND);
}
