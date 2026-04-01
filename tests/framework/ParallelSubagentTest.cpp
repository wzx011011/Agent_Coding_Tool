#include <gtest/gtest.h>

#include "framework/parallel_subagent.h"
#include "framework/subagent_manager.h"

using namespace act::framework;

TEST(ParallelSubagentTest, SpawnParallelSingleConfig)
{
    SubagentManager manager;
    ParallelSubagent parallel(manager);

    SubagentConfig cfg;
    cfg.type = SubagentType::Explore;
    cfg.task = QStringLiteral("Search for all TODO comments");

    ParallelSubagentConfig pCfg;
    pCfg.config = cfg;

    auto results = parallel.spawnParallel({pCfg});
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results[0].success);
    EXPECT_FALSE(results[0].subagentId.isEmpty());
    EXPECT_TRUE(results[0].summary.contains(
        QStringLiteral("completed")));
}

TEST(ParallelSubagentTest, SpawnParallelMultipleConfigs)
{
    SubagentManager manager;
    ParallelSubagent parallel(manager);

    QList<ParallelSubagentConfig> configs;

    for (int i = 0; i < 3; ++i)
    {
        SubagentConfig cfg;
        cfg.type = (i % 2 == 0) ? SubagentType::Explore
                                : SubagentType::Code;
        cfg.task = QStringLiteral("Task %1").arg(i);

        ParallelSubagentConfig pCfg;
        pCfg.config = cfg;
        configs.append(pCfg);
    }

    auto results = parallel.spawnParallel(configs);
    ASSERT_EQ(results.size(), 3);

    for (int i = 0; i < 3; ++i)
    {
        EXPECT_TRUE(results[i].success);
        EXPECT_FALSE(results[i].subagentId.isEmpty());
    }
}

TEST(ParallelSubagentTest, AllCompletedAfterSpawn)
{
    SubagentManager manager;
    ParallelSubagent parallel(manager);

    SubagentConfig cfg;
    cfg.task = QStringLiteral("test task");

    ParallelSubagentConfig pCfg;
    pCfg.config = cfg;

    parallel.spawnParallel({pCfg, pCfg});
    EXPECT_TRUE(parallel.allCompleted());
}

TEST(ParallelSubagentTest, ResultsAreAccessible)
{
    SubagentManager manager;
    ParallelSubagent parallel(manager);

    SubagentConfig cfg;
    cfg.task = QStringLiteral("test");

    ParallelSubagentConfig pCfg;
    pCfg.config = cfg;

    auto spawnResults = parallel.spawnParallel({pCfg});
    auto storedResults = parallel.results();

    ASSERT_EQ(storedResults.size(), 1);
    EXPECT_EQ(storedResults[0].subagentId,
              spawnResults[0].subagentId);
    EXPECT_TRUE(storedResults[0].success);
}

TEST(ParallelSubagentTest, EmptyConfigsReturnsEmptyResults)
{
    SubagentManager manager;
    ParallelSubagent parallel(manager);

    auto results = parallel.spawnParallel({});
    EXPECT_TRUE(results.isEmpty());
    EXPECT_TRUE(parallel.results().isEmpty());
}

TEST(ParallelSubagentTest, AllCompletedWithNoResults)
{
    SubagentManager manager;
    ParallelSubagent parallel(manager);

    // No spawn yet — vacuously true
    EXPECT_TRUE(parallel.allCompleted());
}

TEST(ParallelSubagentTest, SubagentManagerTracksSpawnedAgents)
{
    SubagentManager manager;
    ParallelSubagent parallel(manager);

    SubagentConfig cfg;
    cfg.task = QStringLiteral("tracked task");

    ParallelSubagentConfig pCfg;
    pCfg.config = cfg;

    parallel.spawnParallel({pCfg});

    // The manager should have 1 sub-agent tracked
    EXPECT_EQ(manager.count(), 1);
    auto ids = manager.listSubagents();
    ASSERT_EQ(ids.size(), 1);
    EXPECT_TRUE(manager.isCompleted(ids[0]));
}

TEST(ParallelSubagentTest, DifferentSubagentTypes)
{
    SubagentManager manager;
    ParallelSubagent parallel(manager);

    QList<ParallelSubagentConfig> configs;

    SubagentConfig exploreCfg;
    exploreCfg.type = SubagentType::Explore;
    exploreCfg.task = QStringLiteral("Explore task");
    ParallelSubagentConfig pExplore;
    pExplore.config = exploreCfg;

    SubagentConfig codeCfg;
    codeCfg.type = SubagentType::Code;
    codeCfg.task = QStringLiteral("Code task");
    ParallelSubagentConfig pCode;
    pCode.config = codeCfg;

    auto results = parallel.spawnParallel({pExplore, pCode});
    ASSERT_EQ(results.size(), 2);
    EXPECT_TRUE(results[0].success);
    EXPECT_TRUE(results[1].success);
}
