#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <thread>
#include <vector>

#include "harness/task_manager.h"

using namespace act::harness;

class TaskManagerTest : public ::testing::Test
{
protected:
    TaskManager manager;
};

// --- Create / Get ---

TEST_F(TaskManagerTest, CreateTaskReturnsId)
{
    auto id = manager.createTask(QStringLiteral("My task"));
    EXPECT_FALSE(id.isEmpty());
}

TEST_F(TaskManagerTest, CreateTaskSequentialIds)
{
    auto id1 = manager.createTask(QStringLiteral("First"));
    auto id2 = manager.createTask(QStringLiteral("Second"));
    EXPECT_NE(id1, id2);
}

TEST_F(TaskManagerTest, CreateTaskWithDescription)
{
    auto id = manager.createTask(QStringLiteral("Title"),
                                 QStringLiteral("Detailed desc"));
    auto task = manager.getTask(id);
    EXPECT_EQ(task.subject, QStringLiteral("Title"));
    EXPECT_EQ(task.description, QStringLiteral("Detailed desc"));
}

TEST_F(TaskManagerTest, CreateTaskWithActiveForm)
{
    auto id = manager.createTask(QStringLiteral("Build"),
                                 QStringLiteral("Build project"),
                                 QStringLiteral("Building project"));
    auto task = manager.getTask(id);
    EXPECT_EQ(task.activeForm, QStringLiteral("Building project"));
}

TEST_F(TaskManagerTest, CreateTaskDefaultStatus)
{
    auto id = manager.createTask(QStringLiteral("Task"));
    auto task = manager.getTask(id);
    EXPECT_EQ(task.status, QStringLiteral("pending"));
}

TEST_F(TaskManagerTest, CreateTaskHasTimestamps)
{
    auto id = manager.createTask(QStringLiteral("Task"));
    auto task = manager.getTask(id);
    EXPECT_TRUE(task.createdAt.isValid());
    EXPECT_TRUE(task.updatedAt.isValid());
}

TEST_F(TaskManagerTest, GetTaskNotFound)
{
    auto task = manager.getTask(QStringLiteral("999"));
    EXPECT_TRUE(task.id.isEmpty());
}

TEST_F(TaskManagerTest, HasTask)
{
    auto id = manager.createTask(QStringLiteral("Exists"));
    EXPECT_TRUE(manager.hasTask(id));
    EXPECT_FALSE(manager.hasTask(QStringLiteral("nonexistent")));
}

// --- Update ---

TEST_F(TaskManagerTest, UpdateTaskStatus)
{
    auto id = manager.createTask(QStringLiteral("Task"));
    bool ok = manager.updateTask(id, QStringLiteral("in_progress"));
    EXPECT_TRUE(ok);

    auto task = manager.getTask(id);
    EXPECT_EQ(task.status, QStringLiteral("in_progress"));
}

TEST_F(TaskManagerTest, UpdateTaskDescription)
{
    auto id = manager.createTask(QStringLiteral("Task"));
    bool ok = manager.updateTask(id, {}, QStringLiteral("New desc"));
    EXPECT_TRUE(ok);

    auto task = manager.getTask(id);
    EXPECT_EQ(task.description, QStringLiteral("New desc"));
}

TEST_F(TaskManagerTest, UpdateTaskBlockedBy)
{
    auto id1 = manager.createTask(QStringLiteral("First"));
    auto id2 = manager.createTask(QStringLiteral("Second"));

    bool ok = manager.updateTask(id2, {}, {},
                                 QStringList{id1});
    EXPECT_TRUE(ok);

    auto task2 = manager.getTask(id2);
    ASSERT_EQ(task2.blockedBy.size(), 1);
    EXPECT_EQ(task2.blockedBy.at(0), id1);
}

TEST_F(TaskManagerTest, UpdateTaskBlockedByNoDuplicates)
{
    auto id1 = manager.createTask(QStringLiteral("First"));
    auto id2 = manager.createTask(QStringLiteral("Second"));

    manager.updateTask(id2, {}, {}, QStringList{id1});
    manager.updateTask(id2, {}, {}, QStringList{id1});

    auto task2 = manager.getTask(id2);
    EXPECT_EQ(task2.blockedBy.size(), 1);
}

TEST_F(TaskManagerTest, UpdateTaskActiveForm)
{
    auto id = manager.createTask(QStringLiteral("Task"));
    bool ok = manager.updateTask(id, {}, {}, {},
                                 QStringLiteral("Testing"));
    EXPECT_TRUE(ok);

    auto task = manager.getTask(id);
    EXPECT_EQ(task.activeForm, QStringLiteral("Testing"));
}

TEST_F(TaskManagerTest, UpdateTaskNotFound)
{
    bool ok = manager.updateTask(QStringLiteral("999"),
                                 QStringLiteral("in_progress"));
    EXPECT_FALSE(ok);
}

TEST_F(TaskManagerTest, UpdateTaskUpdatesTimestamp)
{
    auto id = manager.createTask(QStringLiteral("Task"));
    auto before = manager.getTask(id).updatedAt;

    // Small delay to ensure timestamp difference
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    manager.updateTask(id, QStringLiteral("in_progress"));
    auto after = manager.getTask(id).updatedAt;

    EXPECT_GE(after, before);
}

// --- List ---

TEST_F(TaskManagerTest, ListAllTasks)
{
    manager.createTask(QStringLiteral("A"));
    manager.createTask(QStringLiteral("B"));

    auto tasks = manager.listTasks();
    EXPECT_EQ(tasks.size(), 2);
}

TEST_F(TaskManagerTest, ListFilteredByStatus)
{
    auto id1 = manager.createTask(QStringLiteral("A"));
    manager.createTask(QStringLiteral("B"));

    manager.updateTask(id1, QStringLiteral("in_progress"));

    auto pending = manager.listTasks(QStringLiteral("pending"));
    EXPECT_EQ(pending.size(), 1);
    EXPECT_EQ(pending.at(0).subject, QStringLiteral("B"));

    auto inProgress = manager.listTasks(QStringLiteral("in_progress"));
    EXPECT_EQ(inProgress.size(), 1);
    EXPECT_EQ(inProgress.at(0).subject, QStringLiteral("A"));
}

TEST_F(TaskManagerTest, ListEmpty)
{
    auto tasks = manager.listTasks();
    EXPECT_TRUE(tasks.isEmpty());
}

// --- Delete ---

TEST_F(TaskManagerTest, DeleteTask)
{
    auto id = manager.createTask(QStringLiteral("Task"));
    bool ok = manager.deleteTask(id);
    EXPECT_TRUE(ok);

    auto task = manager.getTask(id);
    EXPECT_EQ(task.status, QStringLiteral("deleted"));
}

TEST_F(TaskManagerTest, DeleteTaskNotFound)
{
    bool ok = manager.deleteTask(QStringLiteral("999"));
    EXPECT_FALSE(ok);
}

TEST_F(TaskManagerTest, DeletedNotInPendingList)
{
    auto id = manager.createTask(QStringLiteral("Task"));
    manager.deleteTask(id);

    auto pending = manager.listTasks(QStringLiteral("pending"));
    EXPECT_EQ(pending.size(), 0);

    auto deleted = manager.listTasks(QStringLiteral("deleted"));
    EXPECT_EQ(deleted.size(), 1);
}

// --- BlockedBy relationships ---

TEST_F(TaskManagerTest, MultipleBlockedBy)
{
    auto id1 = manager.createTask(QStringLiteral("A"));
    auto id2 = manager.createTask(QStringLiteral("B"));
    auto id3 = manager.createTask(QStringLiteral("C"));

    manager.updateTask(id3, {}, {},
                       QStringList{id1, id2});

    auto task3 = manager.getTask(id3);
    EXPECT_EQ(task3.blockedBy.size(), 2);
    EXPECT_TRUE(task3.blockedBy.contains(id1));
    EXPECT_TRUE(task3.blockedBy.contains(id2));
}

// --- Thread safety ---

TEST_F(TaskManagerTest, ConcurrentCreates)
{
    constexpr int kThreads = 8;
    constexpr int kPerThread = 50;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([this]
        {
            for (int i = 0; i < kPerThread; ++i)
                manager.createTask(QStringLiteral("Concurrent task"));
        });
    }

    for (auto &th : threads)
        th.join();

    auto tasks = manager.listTasks();
    EXPECT_EQ(tasks.size(), kThreads * kPerThread);
}
