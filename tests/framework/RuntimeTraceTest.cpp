#include <gtest/gtest.h>

#include <QJsonArray>

#include "core/runtime_event.h"
#include "framework/runtime_event_logger.h"
#include "framework/runtime_trace_store.h"

using namespace act::framework;
using namespace act::core;

// ============================================================
// RuntimeEventLogger Tests
// ============================================================

TEST(RuntimeEventLoggerTest, DefaultConstruction)
{
    RuntimeEventLogger logger;
    EXPECT_TRUE(logger.isEnabled());
    EXPECT_TRUE(logger.logFilePath().isEmpty());
}

TEST(RuntimeEventLoggerTest, SetLogFilePath)
{
    RuntimeEventLogger logger;
    logger.setLogFilePath(QStringLiteral("/tmp/events.log"));
    EXPECT_EQ(logger.logFilePath(), QStringLiteral("/tmp/events.log"));
}

TEST(RuntimeEventLoggerTest, DisablePreventsLogging)
{
    RuntimeEventLogger logger;
    logger.setEnabled(false);
    // Should not crash — just skip
    logger.log(RuntimeEvent::error(QStringLiteral("E"), QStringLiteral("msg")));
    EXPECT_FALSE(logger.isEnabled());
}

TEST(RuntimeEventLoggerTest, LogEventDoesNotCrash)
{
    RuntimeEventLogger logger;
    auto evt = RuntimeEvent::streamToken(QStringLiteral("hello"));
    logger.log(evt);
    auto err = RuntimeEvent::error(QStringLiteral("TEST"), QStringLiteral("test error"));
    logger.log(err);
}

// ============================================================
// RuntimeTraceStore Tests
// ============================================================

TEST(RuntimeTraceStoreTest, BeginTaskReturnsId)
{
    RuntimeTraceStore store;
    auto id = store.beginTask(QStringLiteral("Test task"));
    EXPECT_FALSE(id.isEmpty());
    EXPECT_TRUE(id.startsWith(QStringLiteral("task_")));
    EXPECT_EQ(store.taskCount(), 1);
    EXPECT_EQ(store.currentTaskId(), id);
}

TEST(RuntimeTraceStoreTest, MultipleTasksIncrementId)
{
    RuntimeTraceStore store;
    auto id1 = store.beginTask(QStringLiteral("Task 1"));
    store.endTask();
    auto id2 = store.beginTask(QStringLiteral("Task 2"));
    EXPECT_NE(id1, id2);
    EXPECT_EQ(store.taskCount(), 2);
}

TEST(RuntimeTraceStoreTest, EndTaskClearsCurrent)
{
    RuntimeTraceStore store;
    store.beginTask(QStringLiteral("Task"));
    store.endTask();
    EXPECT_TRUE(store.currentTaskId().isEmpty());
}

TEST(RuntimeTraceStoreTest, RecordEventUnderCurrentTask)
{
    RuntimeTraceStore store;
    auto taskId = store.beginTask(QStringLiteral("Test"));
    store.record(RuntimeEvent::error(QStringLiteral("E"), QStringLiteral("err")));

    auto events = store.events(taskId);
    EXPECT_EQ(events.size(), 1);
    EXPECT_EQ(events.first().type, EventType::ErrorOccurred);
}

TEST(RuntimeTraceStoreTest, RecordIgnoredWhenNoActiveTask)
{
    RuntimeTraceStore store;
    store.record(RuntimeEvent::error(QStringLiteral("E"), QStringLiteral("err")));
    EXPECT_EQ(store.taskCount(), 0);
}

TEST(RuntimeTraceStoreTest, EventsForNonexistentTaskIsEmpty)
{
    RuntimeTraceStore store;
    auto events = store.events(QStringLiteral("no_such_task"));
    EXPECT_TRUE(events.isEmpty());
}

TEST(RuntimeTraceStoreTest, TraceJsonContainsTaskData)
{
    RuntimeTraceStore store;
    auto taskId = store.beginTask(QStringLiteral("My task"));
    store.record(RuntimeEvent::toolCall(QStringLiteral("git_status"), {}));
    store.record(RuntimeEvent::error(QStringLiteral("E"), QStringLiteral("fail")));
    store.endTask();

    auto json = store.traceJson(taskId);
    EXPECT_EQ(json.value(QStringLiteral("taskId")).toString(), taskId);
    EXPECT_EQ(json.value(QStringLiteral("description")).toString(), QStringLiteral("My task"));
    EXPECT_EQ(json.value(QStringLiteral("eventCount")).toInt(), 2);

    auto eventsArr = json.value(QStringLiteral("events")).toArray();
    EXPECT_EQ(eventsArr.size(), 2);
}

TEST(RuntimeTraceStoreTest, TraceJsonForNonexistentTaskIsEmpty)
{
    RuntimeTraceStore store;
    auto json = store.traceJson(QStringLiteral("no_such_task"));
    EXPECT_TRUE(json.isEmpty());
}
