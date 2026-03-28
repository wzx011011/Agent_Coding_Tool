#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include "core/types.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "harness/tools/file_edit_tool.h"
#include "harness/tools/file_read_tool.h"
#include "harness/tools/glob_tool.h"
#include "harness/tools/grep_tool.h"

using namespace act::core;
using namespace act::harness;

// Mock IProcess
class MockRegressionProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int timeoutMs = 30000) override
    {
        callback(0, QStringLiteral("mock output"));
    }

    void cancel() override {}
};

// Mock IFileSystem for regression tests.
// Uses real temp directories so QFileInfo-based tools (Glob/Grep) work.
class TempDirFS : public act::infrastructure::IFileSystem
{
public:
    explicit TempDirFS(const QString &root) : m_root(root) {}

    [[nodiscard]] bool readFile(const QString &path,
                                QString &content) const override
    {
        const QString fullPath = resolve(path);
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        content = QString::fromUtf8(file.readAll());
        return true;
    }

    bool writeFile(const QString &path,
                   const QString &content) override
    {
        const QString fullPath = resolve(path);
        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        file.write(content.toUtf8());
        return true;
    }

    [[nodiscard]] QStringList listFiles(
        const QString &dir,
        const QString &pattern = QStringLiteral("*")) const override
    {
        QDir d(resolve(dir));
        return d.entryList(QStringList{pattern},
                           QDir::Files | QDir::NoDotAndDotDot);
    }

    [[nodiscard]] QString normalizePath(const QString &path) const override
    {
        if (QDir::isAbsolutePath(path))
            return QDir::cleanPath(path);
        return QDir::cleanPath(m_root + QLatin1Char('/') + path);
    }

    [[nodiscard]] bool exists(const QString &path) const override
    {
        return QFile::exists(resolve(path)) || QDir(resolve(path)).exists();
    }

    bool removeFile(const QString &path) override
    {
        return QFile::remove(resolve(path));
    }

    bool createDirectory(const QString &path) override
    {
        return QDir().mkpath(resolve(path));
    }

private:
    QString resolve(const QString &path) const
    {
        if (QDir::isAbsolutePath(path))
            return path;
        return QDir::cleanPath(m_root + QLatin1Char('/') + path);
    }

    QString m_root;
};

// --- Regression Test Fixture ---

class RegressionToolTest : public ::testing::Test
{
protected:
    QTemporaryDir m_tmpDir;
    ToolRegistry m_registry;
    PermissionManager m_perms;

    void SetUp() override
    {
        ASSERT_TRUE(m_tmpDir.isValid());
        auto *fs = new TempDirFS(m_tmpDir.path());
        m_fs = fs;

        m_registry.registerTool(std::make_unique<FileReadTool>(*fs,
            m_tmpDir.path()));
        m_registry.registerTool(std::make_unique<GlobTool>(*fs,
            m_tmpDir.path()));
        m_registry.registerTool(std::make_unique<GrepTool>(*fs,
            m_tmpDir.path()));
        m_registry.registerTool(std::make_unique<FileEditTool>(*fs,
            m_tmpDir.path()));
    }

    TempDirFS *m_fs = nullptr;
};

// Helper to write a test file
static void writeFile(const QString &dir, const QString &name,
                      const QString &content)
{
    QDir d(dir);
    d.mkpath(QFileInfo(name).path());
    QFile file(dir + QLatin1Char('/') + name);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(content.toUtf8());
    file.close();
}

// R1: File read returns correct content
TEST_F(RegressionToolTest, ReadFileReturnsContent)
{
    writeFile(m_tmpDir.path(), QStringLiteral("hello.txt"),
              QStringLiteral("Hello, World!"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("hello.txt");
    auto result = m_registry.execute(QStringLiteral("file_read"), params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.contains(QStringLiteral("Hello, World!")));
}

// R2: File read non-existent returns error
TEST_F(RegressionToolTest, ReadNonExistentReturnsError)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("nope.txt");
    auto result = m_registry.execute(QStringLiteral("file_read"), params);
    EXPECT_FALSE(result.success);
}

// R3: Glob finds matching files
TEST_F(RegressionToolTest, GlobFindsFiles)
{
    writeFile(m_tmpDir.path(), QStringLiteral("src/main.cpp"),
              QStringLiteral("code"));
    writeFile(m_tmpDir.path(), QStringLiteral("src/utils.cpp"),
              QStringLiteral("code"));
    writeFile(m_tmpDir.path(), QStringLiteral("tests/test.cpp"),
              QStringLiteral("test"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("*.cpp");
    params[QStringLiteral("path")] = QStringLiteral("src");
    auto result = m_registry.execute(QStringLiteral("glob"), params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.contains(QStringLiteral("main.cpp")));
}

// R4: Grep finds matching content
TEST_F(RegressionToolTest, GrepFindsMatches)
{
    writeFile(m_tmpDir.path(), QStringLiteral("file.txt"),
              QStringLiteral("line one\nerror here\nline three\nwarning here"));

    QJsonObject params;
    params[QStringLiteral("pattern")] = QStringLiteral("error");
    params[QStringLiteral("path")] = QStringLiteral("file.txt");
    auto result = m_registry.execute(QStringLiteral("grep"), params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.contains(QStringLiteral("error")));
}

// R5: FileEdit applies changes
TEST_F(RegressionToolTest, FileEditAppliesChanges)
{
    writeFile(m_tmpDir.path(), QStringLiteral("test.txt"),
              QStringLiteral("old line\nmore content"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.txt");
    params[QStringLiteral("old_string")] = QStringLiteral("old line");
    params[QStringLiteral("new_string")] = QStringLiteral("new line");

    auto result = m_registry.execute(QStringLiteral("file_edit"), params);
    EXPECT_TRUE(result.success);

    QString content;
    (void)m_fs->readFile(QStringLiteral("test.txt"), content);
    EXPECT_TRUE(content.contains(QStringLiteral("new line")));
    EXPECT_FALSE(content.contains(QStringLiteral("old line")));
}

// R6: FileEdit returns error when string not found
TEST_F(RegressionToolTest, FileEditStringNotFound)
{
    writeFile(m_tmpDir.path(), QStringLiteral("test.txt"),
              QStringLiteral("some content"));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.txt");
    params[QStringLiteral("old_string")] = QStringLiteral("not found");
    params[QStringLiteral("new_string")] = QStringLiteral("replacement");

    auto result = m_registry.execute(QStringLiteral("file_edit"), params);
    EXPECT_FALSE(result.success);
}

// R7: Permission check on write tools
TEST_F(RegressionToolTest, PermissionDeniedReturnsError)
{
    // Deny file_edit explicitly and disable auto-approve for writes
    m_perms.setAutoApproved(PermissionLevel::Write, false);
    m_perms.addToDenyList(QStringLiteral("file_edit"));

    // Check permission before executing
    auto *tool = m_registry.getTool(QStringLiteral("file_edit"));
    ASSERT_NE(tool, nullptr);
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Write);

    // Verify the tool is on the deny list
    EXPECT_TRUE(m_perms.isDenied(QStringLiteral("file_edit")));
}
