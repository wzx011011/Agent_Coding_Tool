#include <gtest/gtest.h>

#include "framework/execution_lane.h"

using namespace act::framework;

class ExecutionLaneTest : public ::testing::Test
{
protected:
    ExecutionLaneManager mgr;
};

TEST_F(ExecutionLaneTest, CreateLane)
{
    auto id = mgr.createLane(QStringLiteral("/workspace"));
    EXPECT_FALSE(id.isEmpty());
    EXPECT_TRUE(mgr.hasLane(id));
}

TEST_F(ExecutionLaneTest, CreateMultipleLanes)
{
    auto id1 = mgr.createLane(QStringLiteral("/ws1"));
    auto id2 = mgr.createLane(QStringLiteral("/ws2"));
    EXPECT_NE(id1, id2);
    EXPECT_EQ(mgr.laneIds().size(), 2);
}

TEST_F(ExecutionLaneTest, RemoveLane)
{
    auto id = mgr.createLane(QStringLiteral("/workspace"));
    EXPECT_TRUE(mgr.removeLane(id));
    EXPECT_FALSE(mgr.hasLane(id));
}

TEST_F(ExecutionLaneTest, RemoveNonExistentLane)
{
    EXPECT_FALSE(mgr.removeLane(QStringLiteral("nope")));
}

TEST_F(ExecutionLaneTest, GetLaneDetails)
{
    auto id = mgr.createLane(QStringLiteral("/my-workspace"),
                                   QStringLiteral("feature/test"));
    auto *lane = mgr.lane(id);
    ASSERT_NE(lane, nullptr);
    EXPECT_EQ(lane->workspaceDir, QStringLiteral("/my-workspace"));
    EXPECT_EQ(lane->branchName, QStringLiteral("feature/test"));
    EXPECT_TRUE(lane->isActive);
}

TEST_F(ExecutionLaneTest, AssignTaskToLane)
{
    auto id = mgr.createLane(QStringLiteral("/workspace"));

    TaskNode task;
    task.id = QStringLiteral("task-1");
    task.title = QStringLiteral("Build project");

    EXPECT_TRUE(mgr.assignTask(id, task));

    auto *lane = mgr.lane(id);
    ASSERT_NE(lane, nullptr);
    EXPECT_EQ(lane->taskGraph.size(), 1);
    EXPECT_EQ(lane->taskGraph.listTasks().first(), QStringLiteral("task-1"));
}

TEST_F(ExecutionLaneTest, AssignTaskToNonExistentLane)
{
    TaskNode task;
    task.id = QStringLiteral("task-1");
    EXPECT_FALSE(mgr.assignTask(QStringLiteral("no-lane"), task));
}

TEST_F(ExecutionLaneTest, WorkspaceFor)
{
    auto id = mgr.createLane(QStringLiteral("/custom-workspace"));
    EXPECT_EQ(mgr.workspaceFor(id), QStringLiteral("/custom-workspace"));
    EXPECT_TRUE(mgr.workspaceFor(QStringLiteral("nope")).isEmpty());
}

TEST_F(ExecutionLaneTest, ActiveCount)
{
    EXPECT_EQ(mgr.activeCount(), 0);
    auto id1 = mgr.createLane(QStringLiteral("/ws1"));
    auto id2 = mgr.createLane(QStringLiteral("/ws2"));
    EXPECT_EQ(mgr.activeCount(), 2);

    mgr.removeLane(id1);
    EXPECT_EQ(mgr.activeCount(), 1);
}

TEST_F(ExecutionLaneTest, LaneIdsOrdered)
{
    auto id1 = mgr.createLane(QStringLiteral("/a"));
    auto id2 = mgr.createLane(QStringLiteral("/b"));
    auto ids = mgr.laneIds();
    EXPECT_EQ(ids.size(), 2);
    EXPECT_EQ(ids[0], id1);
    EXPECT_EQ(ids[1], id2);
}
