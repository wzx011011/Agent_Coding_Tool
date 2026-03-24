#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "framework/eval_runner.h"

using namespace act::framework;

class EvalRunnerTest : public ::testing::Test
{
protected:
    EvalRunner runner;
};

TEST_F(EvalRunnerTest, AddTests)
{
    runner.addTest(QStringLiteral("test_read_file"));
    runner.addTest(QStringLiteral("test_grep"));
    runner.addTest(QStringLiteral("test_edit"));

    EXPECT_EQ(runner.totalCount(), 0); // Not run yet
}

TEST_F(EvalRunnerTest, RunAllPassingTests)
{
    runner.addTest(QStringLiteral("test_a"));
    runner.addTest(QStringLiteral("test_b"));
    runner.addTest(QStringLiteral("test_c"));

    runner.run([](const QString &, QString &) { return true; });

    EXPECT_EQ(runner.totalCount(), 3);
    EXPECT_EQ(runner.passCount(), 3);
    EXPECT_EQ(runner.failCount(), 0);
}

TEST_F(EvalRunnerTest, RunWithMixedResults)
{
    runner.addTest(QStringLiteral("pass_1"));
    runner.addTest(QStringLiteral("fail_1"));
    runner.addTest(QStringLiteral("pass_2"));

    runner.run([](const QString &name, QString &err)
    {
        if (name.startsWith(QStringLiteral("fail_")))
        {
            err = QStringLiteral("assertion failed");
            return false;
        }
        return true;
    });

    EXPECT_EQ(runner.totalCount(), 3);
    EXPECT_EQ(runner.passCount(), 2);
    EXPECT_EQ(runner.failCount(), 1);
}

TEST_F(EvalRunnerTest, RunWithNoExecutor)
{
    runner.addTest(QStringLiteral("test_1"));
    runner.run(nullptr);

    EXPECT_EQ(runner.totalCount(), 1);
    EXPECT_EQ(runner.passCount(), 0);
    EXPECT_EQ(runner.failCount(), 0);
}

TEST_F(EvalRunnerTest, ResultsContainsCorrectNames)
{
    runner.addTest(QStringLiteral("alpha"));
    runner.addTest(QStringLiteral("beta"));

    runner.run([](const QString &, QString &) { return true; });

    const auto &results = runner.results();
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].testName, QStringLiteral("alpha"));
    EXPECT_EQ(results[1].testName, QStringLiteral("beta"));
    EXPECT_TRUE(results[0].passed);
    EXPECT_FALSE(results[0].skipped);
}

TEST_F(EvalRunnerTest, ReportGeneratesValidJson)
{
    runner.addTest(QStringLiteral("good_test"));
    runner.addTest(QStringLiteral("bad_test"));

    runner.run([](const QString &name, QString &err)
    {
        if (name == QStringLiteral("bad_test"))
        {
            err = QStringLiteral("error msg");
            return false;
        }
        return true;
    });

    QJsonObject report = runner.report();
    EXPECT_EQ(report.value(QStringLiteral("total")).toInt(), 2);
    EXPECT_EQ(report.value(QStringLiteral("passed")).toInt(), 1);
    EXPECT_EQ(report.value(QStringLiteral("failed")).toInt(), 1);

    QJsonValue resultsVal = report.value(QStringLiteral("results"));
    ASSERT_TRUE(resultsVal.isArray());
    QJsonArray resultsArr = resultsVal.toArray();
    ASSERT_EQ(resultsArr.size(), 2);

    QJsonObject first = resultsArr[0].toObject();
    EXPECT_EQ(first.value(QStringLiteral("name")).toString(),
              QStringLiteral("good_test"));
    EXPECT_EQ(first.value(QStringLiteral("passed")).toBool(), true);

    QJsonObject second = resultsArr[1].toObject();
    EXPECT_EQ(second.value(QStringLiteral("name")).toString(),
              QStringLiteral("bad_test"));
    EXPECT_EQ(second.value(QStringLiteral("passed")).toBool(), false);
    EXPECT_EQ(second.value(QStringLiteral("error")).toString(),
              QStringLiteral("error msg"));
}

TEST_F(EvalRunnerTest, RunReturnsJsonString)
{
    runner.addTest(QStringLiteral("t1"));
    runner.run([](const QString &, QString &) { return true; });

    QString json = runner.run(
        [](const QString &, QString &) { return true; });

    EXPECT_FALSE(json.isEmpty());

    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    EXPECT_FALSE(doc.isNull());
}

TEST_F(EvalRunnerTest, ErrorMsgRecordedInFailedResult)
{
    runner.addTest(QStringLiteral("failing_test"));

    runner.run([](const QString &, QString &err)
    {
        err = QStringLiteral("something went wrong");
        return false;
    });

    const auto &results = runner.results();
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].errorMessage, QStringLiteral("something went wrong"));
}
