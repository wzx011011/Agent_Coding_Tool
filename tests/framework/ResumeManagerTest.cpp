#include <gtest/gtest.h>

#include <QJsonDocument>

#include "core/types.h"
#include "framework/resume_manager.h"

using namespace act::framework;
using namespace act::core;

class ResumeManagerTest : public ::testing::Test
{
protected:
    ResumeManager mgr;

    static AgentLoop::Checkpoint makeCheckpoint(
        TaskState state = TaskState::Running,
        int turnCount = 5,
        int messageCount = 3)
    {
        AgentLoop::Checkpoint cp;
        cp.state = state;
        cp.turnCount = turnCount;

        for (int i = 0; i < messageCount; ++i)
        {
            LLMMessage msg;
            msg.role = (i % 2 == 0) ? MessageRole::User
                                      : MessageRole::Assistant;
            msg.content = QStringLiteral("Message %1").arg(i);
            cp.messages.append(msg);
        }
        return cp;
    }
};

TEST_F(ResumeManagerTest, SaveAndLoad)
{
    auto cp = makeCheckpoint();
    mgr.saveCheckpoint(QStringLiteral("task-1"), cp);

    auto loaded = mgr.loadCheckpoint(QStringLiteral("task-1"));
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->state, TaskState::Running);
    EXPECT_EQ(loaded->turnCount, 5);
    EXPECT_EQ(loaded->messages.size(), 3);
}

TEST_F(ResumeManagerTest, LoadNonExistentReturnsNullopt)
{
    auto result = mgr.loadCheckpoint(QStringLiteral("no-such-task"));
    EXPECT_FALSE(result.has_value());
}

TEST_F(ResumeManagerTest, HasCheckpoint)
{
    EXPECT_FALSE(mgr.hasCheckpoint(QStringLiteral("task-1")));
    mgr.saveCheckpoint(QStringLiteral("task-1"), makeCheckpoint());
    EXPECT_TRUE(mgr.hasCheckpoint(QStringLiteral("task-1")));
}

TEST_F(ResumeManagerTest, RemoveCheckpoint)
{
    mgr.saveCheckpoint(QStringLiteral("task-1"), makeCheckpoint());
    mgr.removeCheckpoint(QStringLiteral("task-1"));
    EXPECT_FALSE(mgr.hasCheckpoint(QStringLiteral("task-1")));
    EXPECT_TRUE(mgr.savedTaskIds().isEmpty());
}

TEST_F(ResumeManagerTest, ClearAll)
{
    mgr.saveCheckpoint(QStringLiteral("task-1"), makeCheckpoint());
    mgr.saveCheckpoint(QStringLiteral("task-2"), makeCheckpoint());
    mgr.clearAll();
    EXPECT_TRUE(mgr.savedTaskIds().isEmpty());
}

TEST_F(ResumeManagerTest, SavedTaskIds)
{
    mgr.saveCheckpoint(QStringLiteral("alpha"), makeCheckpoint());
    mgr.saveCheckpoint(QStringLiteral("beta"), makeCheckpoint());

    auto ids = mgr.savedTaskIds();
    EXPECT_EQ(ids.size(), 2);
    EXPECT_TRUE(ids.contains(QStringLiteral("alpha")));
    EXPECT_TRUE(ids.contains(QStringLiteral("beta")));
}

TEST_F(ResumeManagerTest, OverwriteCheckpoint)
{
    mgr.saveCheckpoint(QStringLiteral("task-1"),
                       makeCheckpoint(TaskState::Running, 5, 3));

    auto cp2 = makeCheckpoint(TaskState::Completed, 10, 6);
    mgr.saveCheckpoint(QStringLiteral("task-1"), cp2);

    auto loaded = mgr.loadCheckpoint(QStringLiteral("task-1"));
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->state, TaskState::Completed);
    EXPECT_EQ(loaded->turnCount, 10);
    EXPECT_EQ(loaded->messages.size(), 6);
}

TEST_F(ResumeManagerTest, SerializeAndDeserialize)
{
    mgr.saveCheckpoint(QStringLiteral("task-a"),
                       makeCheckpoint(TaskState::Failed, 3, 2));

    auto json = mgr.serialize();
    EXPECT_FALSE(json.isEmpty());

    // Create a new manager and deserialize
    ResumeManager mgr2;
    mgr2.deserialize(json);

    auto loaded = mgr2.loadCheckpoint(QStringLiteral("task-a"));
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->state, TaskState::Failed);
    EXPECT_EQ(loaded->turnCount, 3);
    EXPECT_EQ(loaded->messages.size(), 2);
}

TEST_F(ResumeManagerTest, SerializeWithToolCalls)
{
    AgentLoop::Checkpoint cp;
    cp.state = TaskState::Running;
    cp.turnCount = 1;

    LLMMessage msg;
    msg.role = MessageRole::Assistant;
    msg.content = QString();
    msg.toolCall.id = QStringLiteral("call_abc");
    msg.toolCall.name = QStringLiteral("FileReadTool");
    msg.toolCall.params[QStringLiteral("path")] =
        QStringLiteral("/src/main.cpp");
    cp.messages.append(msg);

    mgr.saveCheckpoint(QStringLiteral("tool-task"), cp);

    auto loaded = mgr.loadCheckpoint(QStringLiteral("tool-task"));
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->messages.size(), 1);
    EXPECT_EQ(loaded->messages[0].toolCall.id,
              QStringLiteral("call_abc"));
    EXPECT_EQ(loaded->messages[0].toolCall.name,
              QStringLiteral("FileReadTool"));
}
