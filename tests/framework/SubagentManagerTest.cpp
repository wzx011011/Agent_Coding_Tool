#include <gtest/gtest.h>

#include <QJsonArray>

#include "framework/subagent_manager.h"

using namespace act::framework;

TEST(SubagentManagerTest, SpawnReturnsId)
{
    SubagentManager mgr;
    SubagentConfig config;
    config.type = SubagentType::Explore;
    config.task = QStringLiteral("Find the main entry point");

    auto id = mgr.spawn(config);
    EXPECT_FALSE(id.isEmpty());
    EXPECT_TRUE(id.startsWith(QStringLiteral("sub_")));
    EXPECT_EQ(mgr.count(), 1);
}

TEST(SubagentManagerTest, MultipleSpawnsReturnUniqueIds)
{
    SubagentManager mgr;
    SubagentConfig config;

    auto id1 = mgr.spawn(config);
    auto id2 = mgr.spawn(config);
    EXPECT_NE(id1, id2);
    EXPECT_EQ(mgr.count(), 2);
}

TEST(SubagentManagerTest, NotCompletedInitially)
{
    SubagentManager mgr;
    auto id = mgr.spawn({});
    EXPECT_FALSE(mgr.isCompleted(id));
}

TEST(SubagentManagerTest, CompleteMarksSubagentDone)
{
    SubagentManager mgr;
    auto id = mgr.spawn({});
    EXPECT_FALSE(mgr.isCompleted(id));

    SubagentResult result;
    result.subagentId = id;
    result.type = SubagentType::Explore;
    result.success = true;
    result.summary = QStringLiteral("Found the entry point");

    mgr.complete(id, result);
    EXPECT_TRUE(mgr.isCompleted(id));
}

TEST(SubagentManagerTest, ResultRetrievableAfterComplete)
{
    SubagentManager mgr;
    auto id = mgr.spawn({});

    SubagentResult result;
    result.subagentId = id;
    result.success = true;
    result.summary = QStringLiteral("Done");
    result.structured[QStringLiteral("files")] =
        QJsonArray{QStringLiteral("main.cpp")};

    mgr.complete(id, result);

    auto retrieved = mgr.result(id);
    EXPECT_TRUE(retrieved.success);
    EXPECT_EQ(retrieved.summary, QStringLiteral("Done"));
    EXPECT_EQ(retrieved.subagentId, id);
}

TEST(SubagentManagerTest, ResultForIncompleteIsEmpty)
{
    SubagentManager mgr;
    auto id = mgr.spawn({});
    auto retrieved = mgr.result(id);
    EXPECT_FALSE(retrieved.success);
    EXPECT_TRUE(retrieved.summary.isEmpty());
}

TEST(SubagentManagerTest, ResultForNonexistentIsEmpty)
{
    SubagentManager mgr;
    auto retrieved = mgr.result(QStringLiteral("nonexistent"));
    EXPECT_FALSE(retrieved.success);
}

TEST(SubagentManagerTest, CompletionCallbackFired)
{
    SubagentManager mgr;
    bool called = false;
    QString receivedSummary;

    mgr.setCompletionCallback([&](const SubagentResult &r) {
        called = true;
        receivedSummary = r.summary;
    });

    auto id = mgr.spawn({});
    SubagentResult result;
    result.subagentId = id;
    result.success = true;
    result.summary = QStringLiteral("Callback test");
    mgr.complete(id, result);

    EXPECT_TRUE(called);
    EXPECT_EQ(receivedSummary, QStringLiteral("Callback test"));
}

TEST(SubagentManagerTest, ListSubagents)
{
    SubagentManager mgr;
    mgr.spawn({});
    mgr.spawn({});
    mgr.spawn({});

    auto ids = mgr.listSubagents();
    EXPECT_EQ(ids.size(), 3);
}

TEST(SubagentManagerTest, DefaultToolsExplore)
{
    SubagentManager mgr;
    auto tools = mgr.defaultTools(SubagentType::Explore);
    EXPECT_TRUE(tools.contains(QStringLiteral("file_read")));
    EXPECT_TRUE(tools.contains(QStringLiteral("grep")));
    EXPECT_TRUE(tools.contains(QStringLiteral("glob")));
    EXPECT_FALSE(tools.contains(QStringLiteral("file_write")));
}

TEST(SubagentManagerTest, DefaultToolsCode)
{
    SubagentManager mgr;
    auto tools = mgr.defaultTools(SubagentType::Code);
    EXPECT_TRUE(tools.isEmpty());
}

TEST(SubagentManagerTest, CompleteNonexistentDoesNothing)
{
    SubagentManager mgr;
    SubagentResult result;
    result.subagentId = QStringLiteral("ghost");
    result.success = true;
    // Should not crash
    mgr.complete(QStringLiteral("ghost"), result);
}
