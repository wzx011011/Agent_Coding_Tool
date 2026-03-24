#include <gtest/gtest.h>

#include "framework/patch_transaction.h"

using namespace act::framework;

class PatchTransactionTest : public ::testing::Test
{
protected:
    PatchTransaction tx;
};

TEST_F(PatchTransactionTest, AddPatchIncreasesSize)
{
    tx.addPatch(QStringLiteral("file.txt"),
                QStringLiteral("old"), QStringLiteral("new"));
    EXPECT_EQ(tx.size(), 1);
    EXPECT_TRUE(tx.hasFile(QStringLiteral("file.txt")));
}

TEST_F(PatchTransactionTest, DiffForFileShowsChanges)
{
    tx.addPatch(QStringLiteral("src/main.cpp"),
                QStringLiteral("int x = 1;\n"), QStringLiteral("int x = 2;\n"));

    QString diff = tx.diffForFile(QStringLiteral("src/main.cpp"));
    EXPECT_FALSE(diff.isEmpty());
    EXPECT_TRUE(diff.contains(QStringLiteral("--- a/src/main.cpp")));
    EXPECT_TRUE(diff.contains(QStringLiteral("+++ b/src/main.cpp")));
    EXPECT_TRUE(diff.contains(QStringLiteral("-int x = 1;")));
    EXPECT_TRUE(diff.contains(QStringLiteral("+int x = 2;")));
}

TEST_F(PatchTransactionTest, DiffForNonExistentFile)
{
    QString diff = tx.diffForFile(QStringLiteral("nope.txt"));
    EXPECT_TRUE(diff.isEmpty());
}

TEST_F(PatchTransactionTest, DiffForNoChanges)
{
    tx.addPatch(QStringLiteral("same.txt"),
                QStringLiteral("unchanged"), QStringLiteral("unchanged"));

    QString diff = tx.diffForFile(QStringLiteral("same.txt"));
    EXPECT_TRUE(diff.contains(QStringLiteral("no changes")));
}

TEST_F(PatchTransactionTest, AcceptAndRetrieveContent)
{
    tx.addPatch(QStringLiteral("a.txt"),
                QStringLiteral("before"), QStringLiteral("after"));
    tx.accept(QStringLiteral("a.txt"));

    QString content = tx.newContentFor(QStringLiteral("a.txt"));
    EXPECT_EQ(content, QStringLiteral("after"));
}

TEST_F(PatchTransactionTest, RejectedFileReturnsEmpty)
{
    tx.addPatch(QStringLiteral("a.txt"),
                QStringLiteral("before"), QStringLiteral("after"));
    tx.reject(QStringLiteral("a.txt"));

    QString content = tx.newContentFor(QStringLiteral("a.txt"));
    EXPECT_TRUE(content.isEmpty());
}

TEST_F(PatchTransactionTest, AcceptRejectNonExistent)
{
    EXPECT_FALSE(tx.accept(QStringLiteral("nope")));
    EXPECT_FALSE(tx.reject(QStringLiteral("nope")));
}

TEST_F(PatchTransactionTest, AcceptAll)
{
    tx.addPatch(QStringLiteral("a.txt"), QStringLiteral(""), QStringLiteral("1"));
    tx.addPatch(QStringLiteral("b.txt"), QStringLiteral(""), QStringLiteral("2"));

    tx.acceptAll();
    EXPECT_EQ(tx.acceptedCount(), 2);
    EXPECT_EQ(tx.newContentFor(QStringLiteral("a.txt")), QStringLiteral("1"));
    EXPECT_EQ(tx.newContentFor(QStringLiteral("b.txt")), QStringLiteral("2"));
}

TEST_F(PatchTransactionTest, RejectAll)
{
    tx.addPatch(QStringLiteral("a.txt"), QStringLiteral(""), QStringLiteral("1"));
    tx.rejectAll();
    EXPECT_EQ(tx.rejectedCount(), 1);
    EXPECT_EQ(tx.newContentFor(QStringLiteral("a.txt")), QStringLiteral(""));
}

TEST_F(PatchTransactionTest, DiffAllMultipleFiles)
{
    tx.addPatch(QStringLiteral("a.txt"), QStringLiteral("old_a"), QStringLiteral("new_a"));
    tx.addPatch(QStringLiteral("b.txt"), QStringLiteral("old_b"), QStringLiteral("new_b"));

    QString allDiffs = tx.diffAll();
    EXPECT_TRUE(allDiffs.contains(QStringLiteral("a.txt")));
    EXPECT_TRUE(allDiffs.contains(QStringLiteral("b.txt")));
}

TEST_F(PatchTransactionTest, FilePaths)
{
    tx.addPatch(QStringLiteral("a.txt"), QStringLiteral(""), QStringLiteral("1"));
    tx.addPatch(QStringLiteral("b.txt"), QStringLiteral(""), QStringLiteral("2"));

    auto paths = tx.filePaths();
    EXPECT_EQ(paths.size(), 2);
    EXPECT_TRUE(paths.contains(QStringLiteral("a.txt")));
    EXPECT_TRUE(paths.contains(QStringLiteral("b.txt")));
}

TEST_F(PatchTransactionTest, EntryReturnsCorrectPatch)
{
    tx.addPatch(QStringLiteral("x.txt"), QStringLiteral("orig"), QStringLiteral("mod"));
    auto *entry = tx.entry(QStringLiteral("x.txt"));
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->filePath, QStringLiteral("x.txt"));
    EXPECT_EQ(entry->originalContent, QStringLiteral("orig"));
    EXPECT_EQ(entry->newContent, QStringLiteral("mod"));
}

TEST_F(PatchTransactionTest, EntryReturnsNullForMissing)
{
    EXPECT_EQ(tx.entry(QStringLiteral("missing")), nullptr);
}

// --- v1: Multi-file batch tests ---

TEST_F(PatchTransactionTest, AcceptedPatchesReturnsMap)
{
    tx.addPatch(QStringLiteral("a.txt"), QStringLiteral(""), QStringLiteral("1"));
    tx.addPatch(QStringLiteral("b.txt"), QStringLiteral(""), QStringLiteral("2"));
    tx.addPatch(QStringLiteral("c.txt"), QStringLiteral(""), QStringLiteral("3"));

    tx.accept(QStringLiteral("a.txt"));
    tx.reject(QStringLiteral("b.txt"));

    auto accepted = tx.acceptedPatches();
    EXPECT_EQ(accepted.size(), 1);
    EXPECT_TRUE(accepted.contains(QStringLiteral("a.txt")));
    EXPECT_EQ(accepted.value(QStringLiteral("a.txt")), QStringLiteral("1"));
}

TEST_F(PatchTransactionTest, RejectedPathsReturnsList)
{
    tx.addPatch(QStringLiteral("a.txt"), QStringLiteral(""), QStringLiteral("1"));
    tx.addPatch(QStringLiteral("b.txt"), QStringLiteral(""), QStringLiteral("2"));

    tx.reject(QStringLiteral("b.txt"));

    auto rejected = tx.rejectedPaths();
    EXPECT_EQ(rejected.size(), 1);
    EXPECT_EQ(rejected.first(), QStringLiteral("b.txt"));
}

TEST_F(PatchTransactionTest, BatchSummary)
{
    tx.addPatch(QStringLiteral("a.txt"), QStringLiteral(""), QStringLiteral("1"));
    tx.addPatch(QStringLiteral("b.txt"), QStringLiteral(""), QStringLiteral("2"));

    QString summary = tx.batchSummary();
    EXPECT_TRUE(summary.contains(QStringLiteral("2 files")));
    EXPECT_TRUE(summary.contains(QStringLiteral("0 accepted")));
    EXPECT_TRUE(summary.contains(QStringLiteral("0 rejected")));
    EXPECT_TRUE(summary.contains(QStringLiteral("2 pending")));
}

TEST_F(PatchTransactionTest, ApplyPartialFailure)
{
    tx.addPatch(QStringLiteral("a.txt"), QStringLiteral(""), QStringLiteral("1"));
    tx.addPatch(QStringLiteral("b.txt"), QStringLiteral(""), QStringLiteral("2"));

    tx.applyPartialFailure({QStringLiteral("b.txt")});

    EXPECT_EQ(tx.acceptedCount(), 0);
    EXPECT_EQ(tx.rejectedCount(), 1);  // Only b.txt is rejected
    EXPECT_EQ(tx.pendingCount(), 1);   // a.txt remains pending
}

TEST_F(PatchTransactionTest, PendingCountAfterAccept)
{
    tx.addPatch(QStringLiteral("a.txt"), QStringLiteral(""), QStringLiteral("1"));
    tx.addPatch(QStringLiteral("b.txt"), QStringLiteral(""), QStringLiteral("2"));

    EXPECT_EQ(tx.pendingCount(), 2);
    tx.accept(QStringLiteral("a.txt"));
    EXPECT_EQ(tx.pendingCount(), 1);
}

TEST_F(PatchTransactionTest, AllDecidedWithPending)
{
    tx.addPatch(QStringLiteral("a.txt"), QStringLiteral(""), QStringLiteral("1"));
    EXPECT_FALSE(tx.allDecided());

    tx.accept(QStringLiteral("a.txt"));
    EXPECT_TRUE(tx.allDecided());
}
