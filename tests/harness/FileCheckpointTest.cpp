#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "harness/file_checkpoint.h"

using namespace act::harness;

class FileCheckpointTest : public ::testing::Test
{
protected:
    QTemporaryDir tmpDir;
    std::unique_ptr<FileCheckpoint> checkpoint;

    void SetUp() override
    {
        ASSERT_TRUE(tmpDir.isValid());
        checkpoint = std::make_unique<FileCheckpoint>(tmpDir.path());
    }

    void TearDown() override
    {
        checkpoint.reset();
    }

    void createFile(const QString &relativePath, const QByteArray &content)
    {
        const QString fullPath = tmpDir.path() + QLatin1Char('/') + relativePath;
        QFileInfo fi(fullPath);
        QDir().mkpath(fi.absolutePath());
        QFile f(fullPath);
        (void)f.open(QIODevice::WriteOnly);
        (void)f.write(content);
        f.close();
    }
};

TEST_F(FileCheckpointTest, CheckpointReturnsNonEmptyId)
{
    QString id = checkpoint->checkpoint(QStringLiteral("test.txt"), QByteArray("hello"));
    EXPECT_FALSE(id.isEmpty());
}

TEST_F(FileCheckpointTest, CheckpointCreatesBackupFile)
{
    QString id = checkpoint->checkpoint(QStringLiteral("test.txt"), QByteArray("hello world"));

    auto entries = checkpoint->listCheckpoints();
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].id, id);
    EXPECT_EQ(entries[0].filePath, QStringLiteral("test.txt"));

    // Backup file should exist and contain the original content
    QFile backup(entries[0].backupPath);
    ASSERT_TRUE(backup.exists());
    ASSERT_TRUE(backup.open(QIODevice::ReadOnly));
    EXPECT_EQ(backup.readAll(), QByteArray("hello world"));
}

TEST_F(FileCheckpointTest, RestoreWritesContentBack)
{
    // Create original
    createFile(QStringLiteral("src/main.cpp"), QByteArray("int main() {}"));

    // Checkpoint original content
    QString id = checkpoint->checkpoint(QStringLiteral("src/main.cpp"), QByteArray("int main() {}"));

    // Modify the file
    createFile(QStringLiteral("src/main.cpp"), QByteArray("int main() { return 1; }"));

    // Restore from checkpoint
    EXPECT_TRUE(checkpoint->restore(id));

    // Verify restored content
    QFile f(tmpDir.path() + QStringLiteral("/src/main.cpp"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    EXPECT_EQ(f.readAll(), QByteArray("int main() {}"));
}

TEST_F(FileCheckpointTest, RestoreInvalidIdReturnsFalse)
{
    EXPECT_FALSE(checkpoint->restore(QStringLiteral("nonexistent-id")));
}

TEST_F(FileCheckpointTest, ListCheckpointsReturnsAll)
{
    checkpoint->checkpoint(QStringLiteral("a.txt"), QByteArray("a"));
    checkpoint->checkpoint(QStringLiteral("b.txt"), QByteArray("b"));
    checkpoint->checkpoint(QStringLiteral("c.txt"), QByteArray("c"));

    auto entries = checkpoint->listCheckpoints();
    EXPECT_EQ(entries.size(), 3);
}

TEST_F(FileCheckpointTest, ListCheckpointsFilterByPath)
{
    checkpoint->checkpoint(QStringLiteral("src/a.cpp"), QByteArray("a"));
    checkpoint->checkpoint(QStringLiteral("src/b.cpp"), QByteArray("b"));
    checkpoint->checkpoint(QStringLiteral("docs/readme.md"), QByteArray("r"));

    auto entries = checkpoint->listCheckpoints(QStringLiteral("src/"));
    EXPECT_EQ(entries.size(), 2);
}

TEST_F(FileCheckpointTest, CleanupRemovesOldest)
{
    // Create more than maxCount
    for (int i = 0; i < 55; ++i)
    {
        checkpoint->checkpoint(
            QStringLiteral("file_%1.txt").arg(i),
            QByteArray(QStringLiteral("content_%1").arg(i).toUtf8()));
    }

    int removed = checkpoint->cleanup(50);
    EXPECT_EQ(removed, 5);

    auto entries = checkpoint->listCheckpoints();
    EXPECT_EQ(entries.size(), 50);
}

TEST_F(FileCheckpointTest, CleanupNoopWhenBelowMax)
{
    for (int i = 0; i < 10; ++i)
    {
        checkpoint->checkpoint(
            QStringLiteral("file_%1.txt").arg(i),
            QByteArray("content"));
    }

    int removed = checkpoint->cleanup(50);
    EXPECT_EQ(removed, 0);

    auto entries = checkpoint->listCheckpoints();
    EXPECT_EQ(entries.size(), 10);
}

TEST_F(FileCheckpointTest, CheckpointContentReturnsBackup)
{
    QByteArray content = QByteArray("original content here");
    QString id = checkpoint->checkpoint(QStringLiteral("test.txt"), content);

    QByteArray retrieved = checkpoint->checkpointContent(id);
    EXPECT_EQ(retrieved, content);
}

TEST_F(FileCheckpointTest, CheckpointContentInvalidIdReturnsEmpty)
{
    QByteArray result = checkpoint->checkpointContent(QStringLiteral("no-such-id"));
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(FileCheckpointTest, MultipleCheckpointsForSameFile)
{
    checkpoint->checkpoint(QStringLiteral("file.txt"), QByteArray("v1"));
    checkpoint->checkpoint(QStringLiteral("file.txt"), QByteArray("v2"));
    checkpoint->checkpoint(QStringLiteral("file.txt"), QByteArray("v3"));

    auto entries = checkpoint->listCheckpoints(QStringLiteral("file.txt"));
    ASSERT_EQ(entries.size(), 3);

    // Each should have a different backup
    EXPECT_EQ(checkpoint->checkpointContent(entries[0].id), QByteArray("v1"));
    EXPECT_EQ(checkpoint->checkpointContent(entries[1].id), QByteArray("v2"));
    EXPECT_EQ(checkpoint->checkpointContent(entries[2].id), QByteArray("v3"));
}

TEST_F(FileCheckpointTest, CleanupRemovesBackupFiles)
{
    for (int i = 0; i < 5; ++i)
    {
        checkpoint->checkpoint(
            QStringLiteral("f%1.txt").arg(i),
            QByteArray(QStringLiteral("c%1").arg(i).toUtf8()));
    }

    auto entriesBefore = checkpoint->listCheckpoints();
    ASSERT_EQ(entriesBefore.size(), 5);

    // Keep only 2
    int removed = checkpoint->cleanup(2);
    EXPECT_EQ(removed, 3);

    auto entriesAfter = checkpoint->listCheckpoints();
    EXPECT_EQ(entriesAfter.size(), 2);

    // The removed entries' backup files should no longer exist.
    // On Windows, tight-loop timestamps may be identical so cleanup
    // order is non-deterministic — just verify exactly 3 were removed.
    int removedCount = 0;
    for (int i = 0; i < entriesBefore.size(); ++i)
    {
        if (!QFile::exists(entriesBefore[i].backupPath))
            ++removedCount;
    }
    EXPECT_EQ(removedCount, 3);
}
