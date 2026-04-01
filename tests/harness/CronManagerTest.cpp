#include <gtest/gtest.h>

#include "harness/cron_manager.h"

using namespace act::harness;

TEST(CronManagerTest, CreateCronJob)
{
    CronManager mgr;
    QString id = mgr.createCron(
        QStringLiteral("*/5 * * * *"),
        QStringLiteral("Run tests every 5 min"),
        true);

    EXPECT_FALSE(id.isEmpty());
    EXPECT_TRUE(id.startsWith(QStringLiteral("cron_")));
}

TEST(CronManagerTest, CreateOneShotCron)
{
    CronManager mgr;
    QString id = mgr.createCron(
        QStringLiteral("one-shot"),
        QStringLiteral("Wake up"),
        false);

    EXPECT_FALSE(id.isEmpty());

    auto job = mgr.getCron(id);
    EXPECT_EQ(job.id, id);
    EXPECT_FALSE(job.recurring);
    EXPECT_FALSE(job.fired);
}

TEST(CronManagerTest, ListCrons)
{
    CronManager mgr;
    mgr.createCron(QStringLiteral("cron1"), QStringLiteral("p1"), true);
    mgr.createCron(QStringLiteral("cron2"), QStringLiteral("p2"), true);

    auto jobs = mgr.listCrons();
    EXPECT_EQ(jobs.size(), 2);
}

TEST(CronManagerTest, DeleteCron)
{
    CronManager mgr;
    QString id = mgr.createCron(
        QStringLiteral("cron"), QStringLiteral("p"), true);

    ASSERT_EQ(mgr.listCrons().size(), 1);
    mgr.deleteCron(id);
    EXPECT_TRUE(mgr.listCrons().isEmpty());
}

TEST(CronManagerTest, DeleteNonExistentCron)
{
    CronManager mgr;
    // Should not crash
    mgr.deleteCron(QStringLiteral("nonexistent"));
}

TEST(CronManagerTest, CheckDueCronsRecurring)
{
    CronManager mgr;
    mgr.createCron(QStringLiteral("*/5 * * * *"),
                    QStringLiteral("recurring"), true);

    auto due = mgr.checkDueCrons();
    EXPECT_EQ(due.size(), 1);
}

TEST(CronManagerTest, CheckDueCronsOneShotNotFired)
{
    CronManager mgr;
    QString id = mgr.createCron(
        QStringLiteral("once"), QStringLiteral("one-shot"), false);

    auto due = mgr.checkDueCrons();
    EXPECT_EQ(due.size(), 1);
    EXPECT_EQ(due[0], id);
}

TEST(CronManagerTest, CheckDueCronsOneShotAfterFired)
{
    CronManager mgr;
    QString id = mgr.createCron(
        QStringLiteral("once"), QStringLiteral("one-shot"), false);

    mgr.markFired(id);

    auto due = mgr.checkDueCrons();
    EXPECT_TRUE(due.isEmpty());
}

TEST(CronManagerTest, MarkFiredNonExistent)
{
    CronManager mgr;
    EXPECT_FALSE(mgr.markFired(QStringLiteral("nonexistent")));
}

TEST(CronManagerTest, MarkFiredExisting)
{
    CronManager mgr;
    QString id = mgr.createCron(
        QStringLiteral("once"), QStringLiteral("task"), false);

    EXPECT_TRUE(mgr.markFired(id));

    auto job = mgr.getCron(id);
    EXPECT_TRUE(job.fired);
}

TEST(CronManagerTest, GetCronNotFound)
{
    CronManager mgr;
    auto job = mgr.getCron(QStringLiteral("nonexistent"));
    EXPECT_TRUE(job.id.isEmpty());
}

TEST(CronManagerTest, ParseDurationMinutes)
{
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("5m")), 5);
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("10 min")), 10);
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("30 minutes")), 30);
}

TEST(CronManagerTest, ParseDurationHours)
{
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("1h")), 60);
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("2 hours")), 120);
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("3 hour")), 180);
}

TEST(CronManagerTest, ParseDurationDays)
{
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("1d")), 1440);
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("2 days")), 2880);
}

TEST(CronManagerTest, ParseDurationBareNumber)
{
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("15")), 15);
}

TEST(CronManagerTest, ParseDurationEmpty)
{
    EXPECT_EQ(CronManager::parseDurationToMinutes(QString()), 0);
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("")), 0);
}

TEST(CronManagerTest, ParseDurationSeconds)
{
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("60s")), 1);
    EXPECT_EQ(CronManager::parseDurationToMinutes(QStringLiteral("30 seconds")), 0);
}

TEST(CronManagerTest, ParseAbsoluteTimeToday)
{
    auto result = CronManager::parseAbsoluteTime(QStringLiteral("today"));
    EXPECT_TRUE(result.isValid());
}

TEST(CronManagerTest, ParseAbsoluteTimeTomorrow)
{
    auto result = CronManager::parseAbsoluteTime(QStringLiteral("tomorrow"));
    EXPECT_TRUE(result.isValid());
}

TEST(CronManagerTest, ParseAbsoluteTimeEmpty)
{
    auto result = CronManager::parseAbsoluteTime(QString());
    EXPECT_FALSE(result.isValid());
}

TEST(CronManagerTest, ParseAbsoluteTime12Hour)
{
    auto result = CronManager::parseAbsoluteTime(QStringLiteral("2:30pm"));
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.time().hour(), 14);
    EXPECT_EQ(result.time().minute(), 30);
}

TEST(CronManagerTest, ParseAbsoluteTime24Hour)
{
    auto result = CronManager::parseAbsoluteTime(QStringLiteral("14:30"));
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.time().hour(), 14);
    EXPECT_EQ(result.time().minute(), 30);
}

TEST(CronManagerTest, ParseAbsoluteTime12HourAM)
{
    auto result = CronManager::parseAbsoluteTime(QStringLiteral("9am"));
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.time().hour(), 9);
    EXPECT_EQ(result.time().minute(), 0);
}

TEST(CronManagerTest, ParseAbsoluteTimeInvalid)
{
    auto result =
        CronManager::parseAbsoluteTime(QStringLiteral("invalid"));
    EXPECT_FALSE(result.isValid());
}

TEST(CronManagerTest, MultipleJobsSequentialIds)
{
    CronManager mgr;
    QString id1 = mgr.createCron(QStringLiteral("a"), QStringLiteral("p1"), true);
    QString id2 = mgr.createCron(QStringLiteral("b"), QStringLiteral("p2"), true);
    QString id3 = mgr.createCron(QStringLiteral("c"), QStringLiteral("p3"), false);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_EQ(mgr.listCrons().size(), 3);
}
