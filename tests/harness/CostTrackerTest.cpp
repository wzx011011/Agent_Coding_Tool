#include <gtest/gtest.h>

#include <QJsonArray>

#include "harness/cost_tracker.h"

using namespace act::harness;

TEST(CostTrackerTest, SessionTotalCostStartsAtZero)
{
    CostTracker tracker;
    EXPECT_DOUBLE_EQ(tracker.sessionTotalCost(), 0.0);
}

TEST(CostTrackerTest, SessionTotalTokensStartsAtZero)
{
    CostTracker tracker;
    auto tokens = tracker.sessionTotalTokens();
    EXPECT_EQ(tokens.inputTokens, 0);
    EXPECT_EQ(tokens.outputTokens, 0);
    EXPECT_EQ(tokens.cacheReadTokens, 0);
    EXPECT_EQ(tokens.cacheWriteTokens, 0);
}

TEST(CostTrackerTest, RequestHistoryStartsEmpty)
{
    CostTracker tracker;
    EXPECT_TRUE(tracker.requestHistory().isEmpty());
}

TEST(CostTrackerTest, RecordRequestSingleEntry)
{
    CostTracker tracker;

    TokenUsage usage{1000, 500, 0, 0};
    tracker.recordRequest(QStringLiteral("claude-sonnet-4-6"), usage);

    auto history = tracker.requestHistory();
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0].model.toStdString(), "claude-sonnet-4-6");
    EXPECT_EQ(history[0].tokens.inputTokens, 1000);
    EXPECT_EQ(history[0].tokens.outputTokens, 500);
    EXPECT_GT(history[0].estimatedCost, 0.0);
    EXPECT_TRUE(history[0].timestamp.isValid());
}

TEST(CostTrackerTest, SessionTotalCostAccumulates)
{
    CostTracker tracker;

    TokenUsage usage{1000, 500, 0, 0};
    tracker.recordRequest(QStringLiteral("claude-sonnet-4-6"), usage);
    tracker.recordRequest(QStringLiteral("claude-sonnet-4-6"), usage);

    double cost = tracker.sessionTotalCost();
    double expected = 2.0 * CostTracker::estimateCost(
                                QStringLiteral("claude-sonnet-4-6"), usage);
    EXPECT_NEAR(cost, expected, 0.0001);
}

TEST(CostTrackerTest, SessionTotalTokensAccumulates)
{
    CostTracker tracker;

    TokenUsage usage{100, 50, 10, 5};
    tracker.recordRequest(QStringLiteral("gpt-4o"), usage);
    tracker.recordRequest(QStringLiteral("gpt-4o"), usage);

    auto tokens = tracker.sessionTotalTokens();
    EXPECT_EQ(tokens.inputTokens, 200);
    EXPECT_EQ(tokens.outputTokens, 100);
    EXPECT_EQ(tokens.cacheReadTokens, 20);
    EXPECT_EQ(tokens.cacheWriteTokens, 10);
}

TEST(CostTrackerTest, ResetClearsAll)
{
    CostTracker tracker;

    TokenUsage usage{1000, 500, 0, 0};
    tracker.recordRequest(QStringLiteral("claude-sonnet-4-6"), usage);

    tracker.reset();

    EXPECT_DOUBLE_EQ(tracker.sessionTotalCost(), 0.0);
    EXPECT_TRUE(tracker.requestHistory().isEmpty());
    auto tokens = tracker.sessionTotalTokens();
    EXPECT_EQ(tokens.inputTokens, 0);
}

TEST(CostTrackerTest, SessionSummaryStructure)
{
    CostTracker tracker;

    TokenUsage usage{1000, 500, 50, 20};
    tracker.recordRequest(QStringLiteral("claude-sonnet-4-6"), usage);

    auto summary = tracker.sessionSummary();

    EXPECT_EQ(summary[QStringLiteral("total_requests")].toInt(), 1);
    EXPECT_GT(summary[QStringLiteral("total_cost")].toDouble(), 0.0);

    auto tokensObj =
        summary[QStringLiteral("total_tokens")].toObject();
    EXPECT_EQ(tokensObj[QStringLiteral("input")].toInt(), 1000);
    EXPECT_EQ(tokensObj[QStringLiteral("output")].toInt(), 500);
    EXPECT_EQ(tokensObj[QStringLiteral("cache_read")].toInt(), 50);
    EXPECT_EQ(tokensObj[QStringLiteral("cache_write")].toInt(), 20);

    auto byModel =
        summary[QStringLiteral("cost_by_model")].toObject();
    EXPECT_TRUE(byModel.contains(QStringLiteral("claude-sonnet-4-6")));
}

TEST(CostTrackerTest, SessionSummaryMultipleModels)
{
    CostTracker tracker;

    TokenUsage usage{1000, 500, 0, 0};
    tracker.recordRequest(QStringLiteral("claude-sonnet-4-6"), usage);
    tracker.recordRequest(QStringLiteral("gpt-4o"), usage);

    auto summary = tracker.sessionSummary();

    EXPECT_EQ(summary[QStringLiteral("total_requests")].toInt(), 2);

    auto byModel =
        summary[QStringLiteral("cost_by_model")].toObject();
    EXPECT_TRUE(byModel.contains(QStringLiteral("claude-sonnet-4-6")));
    EXPECT_TRUE(byModel.contains(QStringLiteral("gpt-4o")));
}

TEST(CostTrackerTest, EstimateCostKnownModels)
{
    // claude-sonnet-4-6: input $3.00/1M, output $15.00/1M
    TokenUsage usage{1'000'000, 1'000'000, 0, 0};
    double cost =
        CostTracker::estimateCost(QStringLiteral("claude-sonnet-4-6"), usage);
    EXPECT_NEAR(cost, 3.00 + 15.00, 0.001);

    // claude-opus-4-6: input $15.00/1M, output $75.00/1M
    cost = CostTracker::estimateCost(
        QStringLiteral("claude-opus-4-6"), usage);
    EXPECT_NEAR(cost, 15.00 + 75.00, 0.001);

    // claude-haiku-4-5: input $0.80/1M, output $4.00/1M
    cost = CostTracker::estimateCost(
        QStringLiteral("claude-haiku-4-5"), usage);
    EXPECT_NEAR(cost, 0.80 + 4.00, 0.001);

    // gpt-4o: input $2.50/1M, output $10.00/1M
    cost = CostTracker::estimateCost(
        QStringLiteral("gpt-4o"), usage);
    EXPECT_NEAR(cost, 2.50 + 10.00, 0.001);

    // glm-4-plus: input $1.50/1M, output $7.50/1M
    cost = CostTracker::estimateCost(
        QStringLiteral("glm-4-plus"), usage);
    EXPECT_NEAR(cost, 1.50 + 7.50, 0.001);
}

TEST(CostTrackerTest, EstimateCostUnknownModelUsesDefault)
{
    // Unknown model should use default pricing ($3/$15)
    TokenUsage usage{1'000'000, 1'000'000, 0, 0};
    double cost = CostTracker::estimateCost(
        QStringLiteral("unknown-model"), usage);
    EXPECT_NEAR(cost, 3.00 + 15.00, 0.001);
}

TEST(CostTrackerTest, EstimateCostSmallTokens)
{
    // 1000 input + 500 output with claude-sonnet-4-6 ($3/$15 per 1M)
    TokenUsage usage{1000, 500, 0, 0};
    double cost = CostTracker::estimateCost(
        QStringLiteral("claude-sonnet-4-6"), usage);

    double expectedInput = 1000.0 * 3.00 / 1'000'000.0;
    double expectedOutput = 500.0 * 15.00 / 1'000'000.0;
    EXPECT_NEAR(cost, expectedInput + expectedOutput, 0.0001);
}

TEST(CostTrackerTest, EstimateCostZeroTokens)
{
    TokenUsage usage{0, 0, 0, 0};
    double cost = CostTracker::estimateCost(
        QStringLiteral("claude-sonnet-4-6"), usage);
    EXPECT_DOUBLE_EQ(cost, 0.0);
}

TEST(CostTrackerTest, MultipleRequestsTotalCost)
{
    CostTracker tracker;

    // Record a mix of models
    tracker.recordRequest(
        QStringLiteral("claude-sonnet-4-6"),
        TokenUsage{10'000, 5'000, 0, 0});
    tracker.recordRequest(
        QStringLiteral("gpt-4o"),
        TokenUsage{20'000, 10'000, 0, 0});

    double total = tracker.sessionTotalCost();
    EXPECT_GT(total, 0.0);

    auto history = tracker.requestHistory();
    ASSERT_EQ(history.size(), 2);
}
