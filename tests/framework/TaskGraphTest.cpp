#include <gtest/gtest.h>

#include <QJsonArray>
#include <QTemporaryDir>

#include "framework/task_graph.h"

using namespace act::framework;

static TaskNode makeTask(const QString &id, const QStringList &deps = {})
{
    TaskNode node;
    node.id = id;
    node.title = id;
    node.dependencies = deps;
    return node;
}

// ============================================================
// TaskGraph Tests
// ============================================================

TEST(TaskGraphTest, AddTask)
{
    TaskGraph graph;
    EXPECT_TRUE(graph.addTask(makeTask("T1")));
    EXPECT_EQ(graph.size(), 1);
    EXPECT_NE(graph.findTask("T1"), nullptr);
}

TEST(TaskGraphTest, DuplicateTaskFails)
{
    TaskGraph graph;
    EXPECT_TRUE(graph.addTask(makeTask("T1")));
    EXPECT_FALSE(graph.addTask(makeTask("T1")));
    EXPECT_EQ(graph.size(), 1);
}

TEST(TaskGraphTest, FindNonexistent)
{
    TaskGraph graph;
    EXPECT_EQ(graph.findTask("ghost"), nullptr);
}

TEST(TaskGraphTest, UpdateState)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    EXPECT_TRUE(graph.updateState("T1", TaskNodeState::Running));
    EXPECT_EQ(graph.findTask("T1")->state, TaskNodeState::Running);
}

TEST(TaskGraphTest, UpdateNonexistentFails)
{
    TaskGraph graph;
    EXPECT_FALSE(graph.updateState("ghost", TaskNodeState::Running));
}

TEST(TaskGraphTest, ReadyTasksNoDeps)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.addTask(makeTask("T2"));

    auto ready = graph.readyTasks();
    EXPECT_EQ(ready.size(), 2);
    EXPECT_TRUE(ready.contains("T1"));
    EXPECT_TRUE(ready.contains("T2"));
}

TEST(TaskGraphTest, ReadyTasksWithDeps)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1", {"T0"}));
    graph.addTask(makeTask("T0"));

    // T0 has no deps, is ready. T1 depends on T0.
    auto ready = graph.readyTasks();
    EXPECT_EQ(ready.size(), 1);
    EXPECT_TRUE(ready.contains("T0"));
}

TEST(TaskGraphTest, ReadyTasksAfterCompletion)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1", {"T0"}));
    graph.addTask(makeTask("T0"));
    graph.updateState("T0", TaskNodeState::Completed);

    auto ready = graph.readyTasks();
    EXPECT_EQ(ready.size(), 1);
    EXPECT_TRUE(ready.contains("T1"));
}

TEST(TaskGraphTest, ReadyTasksBlockedByIncompleteDep)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1", {"T0"}));
    graph.addTask(makeTask("T0"));
    graph.updateState("T0", TaskNodeState::Running);

    auto ready = graph.readyTasks();
    EXPECT_TRUE(ready.isEmpty());
}

TEST(TaskGraphTest, ComputeWavesLinear)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.addTask(makeTask("T2", {"T1"}));
    graph.addTask(makeTask("T3", {"T2"}));

    TaskWave waves;
    ASSERT_TRUE(graph.computeWaves(waves));
    ASSERT_EQ(waves.size(), 3);
    EXPECT_EQ(waves[0].size(), 1);
    EXPECT_EQ(waves[1].size(), 1);
    EXPECT_EQ(waves[2].size(), 1);
}

TEST(TaskGraphTest, ComputeWavesParallel)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.addTask(makeTask("T2"));
    graph.addTask(makeTask("T3", {"T1", "T2"}));

    TaskWave waves;
    ASSERT_TRUE(graph.computeWaves(waves));
    ASSERT_EQ(waves.size(), 2);
    EXPECT_EQ(waves[0].size(), 2); // T1 and T2 parallel
    EXPECT_EQ(waves[1].size(), 1); // T3 after both
}

TEST(TaskGraphTest, ComputeWavesComplex)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.addTask(makeTask("T2", {"T1"}));
    graph.addTask(makeTask("T3", {"T1"}));
    graph.addTask(makeTask("T4", {"T2", "T3"}));

    TaskWave waves;
    ASSERT_TRUE(graph.computeWaves(waves));
    ASSERT_EQ(waves.size(), 3);
    EXPECT_EQ(waves[0].size(), 1); // T1
    EXPECT_EQ(waves[1].size(), 2); // T2, T3
    EXPECT_EQ(waves[2].size(), 1); // T4
}

TEST(TaskGraphTest, CycleDetection)
{
    TaskGraph graph;
    graph.addTask(makeTask("A", {"B"}));
    graph.addTask(makeTask("B", {"C"}));
    graph.addTask(makeTask("C", {"A"}));

    TaskWave waves;
    EXPECT_FALSE(graph.computeWaves(waves));
}

TEST(TaskGraphTest, AllCompleted)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.addTask(makeTask("T2"));
    graph.updateState("T1", TaskNodeState::Completed);
    graph.updateState("T2", TaskNodeState::Completed);
    EXPECT_TRUE(graph.allCompleted());
}

TEST(TaskGraphTest, AllCompletedWithSkipped)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.updateState("T1", TaskNodeState::Skipped);
    EXPECT_TRUE(graph.allCompleted());
}

TEST(TaskGraphTest, NotAllCompleted)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.addTask(makeTask("T2"));
    graph.updateState("T1", TaskNodeState::Completed);
    EXPECT_FALSE(graph.allCompleted());
}

TEST(TaskGraphTest, AllCompletedEmptyGraph)
{
    TaskGraph graph;
    EXPECT_FALSE(graph.allCompleted());
}

TEST(TaskGraphTest, TasksByState)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.addTask(makeTask("T2"));
    graph.addTask(makeTask("T3"));
    graph.updateState("T1", TaskNodeState::Completed);
    graph.updateState("T2", TaskNodeState::Failed);

    auto completed = graph.tasksByState(TaskNodeState::Completed);
    EXPECT_EQ(completed.size(), 1);
    EXPECT_TRUE(completed.contains("T1"));

    auto failed = graph.tasksByState(TaskNodeState::Failed);
    EXPECT_EQ(failed.size(), 1);

    auto pending = graph.tasksByState(TaskNodeState::Pending);
    EXPECT_EQ(pending.size(), 1);
}

TEST(TaskGraphTest, UpdateStateWithResult)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.updateState("T1", TaskNodeState::Completed, "commit: abc123");
    EXPECT_EQ(graph.findTask("T1")->result, "commit: abc123");
}

// ============================================================
// TaskStateStore Tests
// ============================================================

TEST(TaskStateStoreTest, SerializeAndDeserialize)
{
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.addTask(makeTask("T2", {"T1"}));
    graph.updateState("T1", TaskNodeState::Completed, "done");

    TaskStateStore store;
    auto json = store.serialize(graph);

    TaskGraph restored = store.deserialize(json);
    EXPECT_EQ(restored.size(), 2);
    EXPECT_NE(restored.findTask("T1"), nullptr);
    EXPECT_NE(restored.findTask("T2"), nullptr);
    EXPECT_EQ(restored.findTask("T1")->state, TaskNodeState::Completed);
    EXPECT_EQ(restored.findTask("T1")->result, "done");
    EXPECT_EQ(restored.findTask("T2")->dependencies.size(), 1);
}

TEST(TaskStateStoreTest, SaveAndLoadFile)
{
    QTemporaryDir tmpDir;
    TaskGraph graph;
    graph.addTask(makeTask("T1"));
    graph.addTask(makeTask("T2", {"T1"}));
    graph.updateState("T1", TaskNodeState::Completed);

    TaskStateStore store;
    QString filePath = tmpDir.path() + QLatin1String("/state.json");
    ASSERT_TRUE(store.saveToFile(graph, filePath));

    TaskGraph loaded = store.loadFromFile(filePath);
    EXPECT_EQ(loaded.size(), 2);
    EXPECT_EQ(loaded.findTask("T1")->state, TaskNodeState::Completed);
}

TEST(TaskStateStoreTest, LoadNonexistentFile)
{
    TaskStateStore store;
    TaskGraph graph = store.loadFromFile(QStringLiteral("/nonexistent/state.json"));
    EXPECT_EQ(graph.size(), 0);
}
