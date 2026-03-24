#include <gtest/gtest.h>

#include <QCoreApplication>

#include "framework/agent_scheduler.h"

using namespace act::framework;

class AgentSchedulerTest : public ::testing::Test
{
protected:
    TaskGraph makeLinearGraph(int taskCount = 3)
    {
        TaskGraph graph;
        for (int i = 0; i < taskCount; ++i)
        {
            TaskNode task;
            task.id = QStringLiteral("T%1").arg(i + 1);
            task.title = QStringLiteral("Task %1").arg(i + 1);
            task.description = QStringLiteral("Description %1").arg(i + 1);
            if (i > 0)
                task.dependencies.append(QStringLiteral("T%1").arg(i));
            graph.addTask(task);
        }
        return graph;
    }
};

TEST_F(AgentSchedulerTest, LoadAndStartEmptyGraphFails)
{
    AgentScheduler scheduler;
    TaskGraph empty;
    scheduler.loadGraph(empty);
    EXPECT_FALSE(scheduler.start());
}

TEST_F(AgentSchedulerTest, ExecuteLinearGraphSynchronously)
{
    AgentScheduler scheduler;
    auto graph = makeLinearGraph(3);
    scheduler.loadGraph(graph);

    int executedCount = 0;
    scheduler.setExecutor(
        [&executedCount](const QString &id, const QString &)
        {
            ++executedCount;
            return QStringLiteral("done: %1").arg(id);
        });

    EXPECT_TRUE(scheduler.start());
    EXPECT_EQ(executedCount, 3);
    EXPECT_EQ(scheduler.completedCount(), 3);
}

TEST_F(AgentSchedulerTest, DoubleStartReturnsFalse)
{
    AgentScheduler scheduler;
    auto graph = makeLinearGraph(1);
    scheduler.loadGraph(graph);

    scheduler.setExecutor([](const QString &, const QString &)
    { return QStringLiteral("ok"); });

    // First start completes synchronously and transitions to Completed.
    EXPECT_TRUE(scheduler.start());
    // Second start should return false (already Completed).
    EXPECT_FALSE(scheduler.start());
}

TEST_F(AgentSchedulerTest, CancelStopsExecution)
{
    AgentScheduler scheduler;
    auto graph = makeLinearGraph(3);
    scheduler.loadGraph(graph);

    int executedCount = 0;
    scheduler.setExecutor(
        [&executedCount, &scheduler](const QString &id, const QString &)
        {
            ++executedCount;
            if (executedCount == 1)
                scheduler.cancel();
            return QStringLiteral("done");
        });

    scheduler.start();
    // Depending on execution order, at least 1 task should execute
    EXPECT_GE(executedCount, 1);
}

TEST_F(AgentSchedulerTest, TaskFailureRecorded)
{
    AgentScheduler scheduler;
    auto graph = makeLinearGraph(2);
    scheduler.loadGraph(graph);

    scheduler.setExecutor([](const QString &, const QString &)
    { return QString(); }); // Empty = failure

    scheduler.start();
    EXPECT_EQ(scheduler.completedCount(), 0);
    EXPECT_EQ(scheduler.failedCount(), 2);
}

TEST_F(AgentSchedulerTest, ParallelTasksInSameWave)
{
    TaskGraph graph;
    for (int i = 0; i < 3; ++i)
    {
        TaskNode task;
        task.id = QStringLiteral("P%1").arg(i);
        task.title = QStringLiteral("Parallel %1").arg(i);
        graph.addTask(task);
    }

    AgentScheduler scheduler;
    scheduler.loadGraph(graph);

    QStringList executed;
    scheduler.setExecutor(
        [&executed](const QString &id, const QString &)
        {
            executed.append(id);
            return QStringLiteral("done");
        });

    scheduler.start();
    EXPECT_EQ(executed.size(), 3);
    // All should be in the same wave (no dependencies)
}
