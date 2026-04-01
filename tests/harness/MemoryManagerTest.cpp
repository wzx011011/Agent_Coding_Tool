#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

#include "harness/memory_manager.h"

using namespace act::harness;

// ============================================================
// MemoryManager Tests
// ============================================================

class MemoryManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(tmpDir.isValid());
        mm = std::make_unique<MemoryManager>(
            tmpDir.path() + QStringLiteral("/memory"));
    }

    void TearDown() override { mm.reset(); }

    QTemporaryDir tmpDir;
    std::unique_ptr<MemoryManager> mm;
};

// ---- ensureDirectoryExists ----

TEST_F(MemoryManagerTest, EnsureDirectoryCreatesStructure)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    const QString base = tmpDir.path() + QStringLiteral("/memory");
    EXPECT_TRUE(QDir(base).exists());
    EXPECT_TRUE(QDir(base + QStringLiteral("/user")).exists());
    EXPECT_TRUE(QDir(base + QStringLiteral("/project")).exists());
    EXPECT_TRUE(QDir(base + QStringLiteral("/feedback")).exists());
    EXPECT_TRUE(QDir(base + QStringLiteral("/reference")).exists());
}

TEST_F(MemoryManagerTest, EnsureDirectoryIdempotent)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());
    EXPECT_TRUE(mm->ensureDirectoryExists());
}

// ---- save / load ----

TEST_F(MemoryManagerTest, SaveAndLoadBasicEntry)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    const QString content = QStringLiteral("# User Role\n\nBackend developer.");
    const QString desc = QStringLiteral("User's primary role");
    ASSERT_TRUE(mm->save(MemoryType::User, QStringLiteral("role"), content, desc));

    const MemoryEntry entry = mm->load(MemoryType::User, QStringLiteral("role"));
    EXPECT_EQ(entry.name, QStringLiteral("role"));
    EXPECT_EQ(entry.type, MemoryType::User);
    EXPECT_EQ(entry.description, desc);
    EXPECT_TRUE(entry.content.contains(QStringLiteral("Backend developer")));
    EXPECT_TRUE(entry.updatedAt.isValid());
}

TEST_F(MemoryManagerTest, SaveCreatesFileOnDisk)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    ASSERT_TRUE(mm->save(MemoryType::Project, QStringLiteral("architecture"),
                         QStringLiteral("Layered architecture")));

    const QString path = tmpDir.path() +
                         QStringLiteral("/memory/project/architecture.md");
    EXPECT_TRUE(QFile::exists(path));
}

TEST_F(MemoryManagerTest, LoadNonexistentReturnsEmpty)
{
    const MemoryEntry entry =
        mm->load(MemoryType::User, QStringLiteral("nonexistent"));
    EXPECT_TRUE(entry.name.isEmpty());
    EXPECT_TRUE(entry.content.isEmpty());
}

TEST_F(MemoryManagerTest, SaveOverwritesExisting)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    ASSERT_TRUE(mm->save(MemoryType::User, QStringLiteral("role"),
                         QStringLiteral("First")));
    ASSERT_TRUE(mm->save(MemoryType::User, QStringLiteral("role"),
                         QStringLiteral("Second")));

    const MemoryEntry entry = mm->load(MemoryType::User, QStringLiteral("role"));
    EXPECT_EQ(entry.content, QStringLiteral("Second"));
}

TEST_F(MemoryManagerTest, SaveEmptyNameFails)
{
    EXPECT_FALSE(mm->save(MemoryType::User, QString(), QStringLiteral("content")));
}

TEST_F(MemoryManagerTest, SaveWithoutDescription)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    ASSERT_TRUE(mm->save(MemoryType::Feedback, QStringLiteral("testing"),
                         QStringLiteral("Always use QTemporaryDir")));

    const MemoryEntry entry =
        mm->load(MemoryType::Feedback, QStringLiteral("testing"));
    EXPECT_EQ(entry.name, QStringLiteral("testing"));
    EXPECT_TRUE(entry.description.isEmpty());
    EXPECT_EQ(entry.content, QStringLiteral("Always use QTemporaryDir"));
}

// ---- remove ----

TEST_F(MemoryManagerTest, RemoveExistingEntry)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    ASSERT_TRUE(mm->save(MemoryType::Reference, QStringLiteral("dashboards"),
                         QStringLiteral("Grafana at localhost:3000")));
    EXPECT_TRUE(mm->remove(MemoryType::Reference, QStringLiteral("dashboards")));

    const MemoryEntry entry =
        mm->load(MemoryType::Reference, QStringLiteral("dashboards"));
    EXPECT_TRUE(entry.name.isEmpty());
}

TEST_F(MemoryManagerTest, RemoveNonexistentReturnsFalse)
{
    EXPECT_FALSE(mm->remove(MemoryType::User, QStringLiteral("nope")));
}

TEST_F(MemoryManagerTest, FileRemovedFromDisk)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    ASSERT_TRUE(mm->save(MemoryType::Project, QStringLiteral("tech_stack"),
                         QStringLiteral("C++20/Qt6")));
    const QString path = tmpDir.path() +
                         QStringLiteral("/memory/project/tech_stack.md");
    EXPECT_TRUE(QFile::exists(path));

    ASSERT_TRUE(mm->remove(MemoryType::Project, QStringLiteral("tech_stack")));
    EXPECT_FALSE(QFile::exists(path));
}

// ---- listByType / listAll ----

TEST_F(MemoryManagerTest, ListByTypeReturnsCorrectEntries)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    mm->save(MemoryType::User, QStringLiteral("role"),
             QStringLiteral("Dev"), QStringLiteral("Role"));
    mm->save(MemoryType::User, QStringLiteral("language"),
             QStringLiteral("C++"), QStringLiteral("Language"));
    mm->save(MemoryType::Project, QStringLiteral("arch"),
             QStringLiteral("Layers"), QStringLiteral("Architecture"));

    const QList<MemoryEntry> userEntries = mm->listByType(MemoryType::User);
    EXPECT_EQ(userEntries.size(), 2);

    const QList<MemoryEntry> projectEntries = mm->listByType(MemoryType::Project);
    EXPECT_EQ(projectEntries.size(), 1);

    const QList<MemoryEntry> feedbackEntries =
        mm->listByType(MemoryType::Feedback);
    EXPECT_EQ(feedbackEntries.size(), 0);
}

TEST_F(MemoryManagerTest, ListAllAggregatesAllTypes)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    mm->save(MemoryType::User, QStringLiteral("u1"), QStringLiteral("user1"));
    mm->save(MemoryType::Project, QStringLiteral("p1"), QStringLiteral("proj1"));
    mm->save(MemoryType::Feedback, QStringLiteral("f1"), QStringLiteral("feed1"));
    mm->save(MemoryType::Reference, QStringLiteral("r1"), QStringLiteral("ref1"));

    const QList<MemoryEntry> all = mm->listAll();
    EXPECT_EQ(all.size(), 4);
}

TEST_F(MemoryManagerTest, ListByTypeEmptyWhenNoEntries)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    const QList<MemoryEntry> entries = mm->listByType(MemoryType::Reference);
    EXPECT_TRUE(entries.isEmpty());
}

// ---- buildMemoryIndex ----

TEST_F(MemoryManagerTest, BuildMemoryIndexEmpty)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    const QString index = mm->buildMemoryIndex();
    EXPECT_TRUE(index.contains(QStringLiteral("No memory entries yet")));
}

TEST_F(MemoryManagerTest, BuildMemoryIndexWithEntries)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    mm->save(MemoryType::User, QStringLiteral("role"),
             QStringLiteral("Developer"), QStringLiteral("User's role"));
    mm->save(MemoryType::Project, QStringLiteral("stack"),
             QStringLiteral("C++20/Qt6"), QStringLiteral("Tech stack"));

    const QString index = mm->buildMemoryIndex();

    // Should contain type headers
    EXPECT_TRUE(index.contains(QStringLiteral("## user")));
    EXPECT_TRUE(index.contains(QStringLiteral("## project")));

    // Should contain entry links
    EXPECT_TRUE(index.contains(QStringLiteral("[role]")));
    EXPECT_TRUE(index.contains(QStringLiteral("[stack]")));

    // Should contain descriptions
    EXPECT_TRUE(index.contains(QStringLiteral("User's role")));
    EXPECT_TRUE(index.contains(QStringLiteral("Tech stack")));
}

TEST_F(MemoryManagerTest, BuildMemoryIndexFormat)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    mm->save(MemoryType::User, QStringLiteral("test"),
             QStringLiteral("Content"), QStringLiteral("Test description"));

    const QString index = mm->buildMemoryIndex();

    // Verify the markdown link format
    EXPECT_TRUE(index.contains(
        QStringLiteral("[test](user/test.md)")));
    EXPECT_TRUE(index.contains(
        QStringLiteral("Test description")));
}

// ---- Thread safety ----

TEST_F(MemoryManagerTest, ConcurrentSavesDoNotCorrupt)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    constexpr int kThreadCount = 4;
    constexpr int kWritesPerThread = 10;

    QList<QThread *> threads;
    for (int t = 0; t < kThreadCount; ++t)
    {
        const int threadId = t;
        auto *thread = QThread::create([this, threadId]() {
            for (int i = 0; i < kWritesPerThread; ++i)
            {
                const QString name =
                    QStringLiteral("t%1_%2").arg(threadId).arg(i);
                mm->save(MemoryType::User, name,
                         QStringLiteral("Content from thread %1").arg(threadId));
            }
        });
        threads.append(thread);
        thread->start();
    }

    for (auto *thread : threads)
    {
        thread->wait(5000);
        EXPECT_FALSE(thread->isRunning());
        delete thread;
    }

    const QList<MemoryEntry> entries = mm->listByType(MemoryType::User);
    EXPECT_EQ(entries.size(), kThreadCount * kWritesPerThread);
}

// ---- Entry type parsing round-trip ----

TEST_F(MemoryManagerTest, LoadParsesTypeFromDirectory)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    mm->save(MemoryType::Feedback, QStringLiteral("prefs"),
             QStringLiteral("No emojis"), QStringLiteral("Style preference"));

    const MemoryEntry entry =
        mm->load(MemoryType::Feedback, QStringLiteral("prefs"));
    EXPECT_EQ(entry.type, MemoryType::Feedback);
}

TEST_F(MemoryManagerTest, SaveMultilineContent)
{
    ASSERT_TRUE(mm->ensureDirectoryExists());

    const QString content = QStringLiteral(
        "# Architecture\n"
        "\n"
        "5-layer separation:\n"
        "\n"
        "- Core\n"
        "- Infrastructure\n"
        "- Services\n"
        "- Harness\n"
        "- Framework");

    ASSERT_TRUE(mm->save(MemoryType::Project, QStringLiteral("arch"), content,
                         QStringLiteral("System architecture")));

    const MemoryEntry entry =
        mm->load(MemoryType::Project, QStringLiteral("arch"));
    EXPECT_TRUE(entry.content.contains(QStringLiteral("5-layer separation")));
    EXPECT_TRUE(entry.content.contains(QStringLiteral("Framework")));
    EXPECT_EQ(entry.description, QStringLiteral("System architecture"));
}
