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

// ============================================================
// RuntimeEvent ModelRequest Tests
// ============================================================

TEST(RuntimeEventTest, ModelRequestCreatesCorrectEvent)
{
    auto evt = RuntimeEvent::modelRequest(
        QStringLiteral("claude-3-opus"), 150, 320, 1200, 1);

    EXPECT_EQ(evt.type, EventType::ModelRequest);
    EXPECT_EQ(evt.data.value(QStringLiteral("model")).toString(),
              QStringLiteral("claude-3-opus"));
    EXPECT_EQ(evt.data.value(QStringLiteral("input_tokens")).toInt(), 150);
    EXPECT_EQ(evt.data.value(QStringLiteral("output_tokens")).toInt(), 320);
    EXPECT_EQ(evt.data.value(QStringLiteral("latency_ms")).toInt(), 1200);
    EXPECT_EQ(evt.data.value(QStringLiteral("turn")).toInt(), 1);
}

TEST(RuntimeEventTest, ModelRequestWithZeroValues)
{
    auto evt = RuntimeEvent::modelRequest(
        QStringLiteral("gpt-4"), 0, 0, 0, 0);

    EXPECT_EQ(evt.type, EventType::ModelRequest);
    EXPECT_EQ(evt.data.value(QStringLiteral("input_tokens")).toInt(), 0);
    EXPECT_EQ(evt.data.value(QStringLiteral("output_tokens")).toInt(), 0);
    EXPECT_EQ(evt.data.value(QStringLiteral("latency_ms")).toInt(), 0);
    EXPECT_EQ(evt.data.value(QStringLiteral("turn")).toInt(), 0);
}

TEST(RuntimeEventLoggerTest, LogModelRequestDoesNotCrash)
{
    RuntimeEventLogger logger;
    auto evt = RuntimeEvent::modelRequest(
        QStringLiteral("claude-3-opus"), 100, 200, 500, 1);
    logger.log(evt);
}

TEST(RuntimeTraceStoreTest, RecordModelRequestUnderCurrentTask)
{
    RuntimeTraceStore store;
    auto taskId = store.beginTask(QStringLiteral("Model request task"));
    store.record(RuntimeEvent::modelRequest(
        QStringLiteral("claude-3-opus"), 50, 100, 300, 1));

    auto events = store.events(taskId);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events.first().type, EventType::ModelRequest);
    EXPECT_EQ(events.first().data.value(QStringLiteral("model")).toString(),
              QStringLiteral("claude-3-opus"));
}

// ============================================================
// RuntimeEvent PermissionAudit Tests
// ============================================================

TEST(RuntimeEventTest, PermissionAuditApprovedCreatesCorrectEvent)
{
    auto evt = RuntimeEvent::permissionAudit(
        QStringLiteral("file_write"),
        QStringLiteral("Write"),
        PermissionAuditResult::Approved,
        QStringLiteral("User approved"));

    EXPECT_EQ(evt.type, EventType::PermissionAudit);
    EXPECT_EQ(evt.data.value(QStringLiteral("tool_name")).toString(),
              QStringLiteral("file_write"));
    EXPECT_EQ(evt.data.value(QStringLiteral("permission_level")).toString(),
              QStringLiteral("Write"));
    EXPECT_EQ(evt.data.value(QStringLiteral("result")).toInt(),
              static_cast<int>(PermissionAuditResult::Approved));
    EXPECT_EQ(evt.data.value(QStringLiteral("reason")).toString(),
              QStringLiteral("User approved"));
}

TEST(RuntimeEventTest, PermissionAuditDenied)
{
    auto evt = RuntimeEvent::permissionAudit(
        QStringLiteral("shell_exec"),
        QStringLiteral("Exec"),
        PermissionAuditResult::Denied,
        QStringLiteral("Security policy"));

    EXPECT_EQ(evt.type, EventType::PermissionAudit);
    EXPECT_EQ(evt.data.value(QStringLiteral("result")).toInt(),
              static_cast<int>(PermissionAuditResult::Denied));
}

TEST(RuntimeEventTest, PermissionAuditAutoApproved)
{
    auto evt = RuntimeEvent::permissionAudit(
        QStringLiteral("file_read"),
        QStringLiteral("Read"),
        PermissionAuditResult::AutoApproved);

    EXPECT_EQ(evt.type, EventType::PermissionAudit);
    EXPECT_EQ(evt.data.value(QStringLiteral("result")).toInt(),
              static_cast<int>(PermissionAuditResult::AutoApproved));
    EXPECT_FALSE(evt.data.contains(QStringLiteral("reason")));
}

TEST(RuntimeEventTest, PermissionAuditOmitsEmptyReason)
{
    auto evt = RuntimeEvent::permissionAudit(
        QStringLiteral("dir_list"),
        QStringLiteral("Read"),
        PermissionAuditResult::AutoApproved);

    EXPECT_FALSE(evt.data.contains(QStringLiteral("reason")));
}

TEST(RuntimeEventLoggerTest, LogPermissionAuditDoesNotCrash)
{
    RuntimeEventLogger logger;
    auto evt = RuntimeEvent::permissionAudit(
        QStringLiteral("shell_exec"),
        QStringLiteral("Exec"),
        PermissionAuditResult::Denied,
        QStringLiteral("Risk too high"));
    logger.log(evt);
}

TEST(RuntimeTraceStoreTest, RecordPermissionAuditUnderCurrentTask)
{
    RuntimeTraceStore store;
    auto taskId = store.beginTask(QStringLiteral("Permission audit task"));
    store.record(RuntimeEvent::permissionAudit(
        QStringLiteral("file_write"),
        QStringLiteral("Write"),
        PermissionAuditResult::Approved,
        QStringLiteral("User approved")));

    auto events = store.events(taskId);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events.first().type, EventType::PermissionAudit);
    EXPECT_EQ(events.first().data.value(QStringLiteral("tool_name")).toString(),
              QStringLiteral("file_write"));
}

TEST(RuntimeTraceStoreTest, TraceJsonWithNewEventTypes)
{
    RuntimeTraceStore store;
    auto taskId = store.beginTask(QStringLiteral("Mixed events"));
    store.record(RuntimeEvent::modelRequest(
        QStringLiteral("gpt-4"), 100, 200, 500, 1));
    store.record(RuntimeEvent::permissionAudit(
        QStringLiteral("shell_exec"),
        QStringLiteral("Exec"),
        PermissionAuditResult::AutoApproved));
    store.endTask();

    auto json = store.traceJson(taskId);
    auto eventsArr = json.value(QStringLiteral("events")).toArray();
    ASSERT_EQ(eventsArr.size(), 2);

    EXPECT_EQ(eventsArr.at(0).toObject().value(QStringLiteral("type")).toInt(),
              static_cast<int>(EventType::ModelRequest));
    EXPECT_EQ(eventsArr.at(1).toObject().value(QStringLiteral("type")).toInt(),
              static_cast<int>(EventType::PermissionAudit));
}
