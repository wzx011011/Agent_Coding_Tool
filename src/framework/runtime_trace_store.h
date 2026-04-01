#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include <mutex>

#include "core/runtime_event.h"

namespace act::framework
{

/// Stores runtime events aggregated by task ID.
/// Thread-safe: all public methods acquire an internal mutex.
class RuntimeTraceStore
{
public:
    /// Start recording events for a new task. Returns the task ID.
    [[nodiscard]] QString beginTask(const QString &description = {});

    /// End the current task.
    void endTask();

    /// Record an event for the current task.
    void record(const act::core::RuntimeEvent &event);

    /// Get all events for a specific task.
    [[nodiscard]] QList<act::core::RuntimeEvent> events(
        const QString &taskId) const;

    /// Get the trace as JSON for a specific task.
    [[nodiscard]] QJsonObject traceJson(const QString &taskId) const;

    /// Get the current active task ID.
    [[nodiscard]] QString currentTaskId() const;

    /// Number of recorded tasks.
    [[nodiscard]] int taskCount() const;

private:
    struct TaskTrace
    {
        QString id;
        QString description;
        QList<act::core::RuntimeEvent> events;
    };

    [[nodiscard]] QString generateTaskId();

    mutable std::mutex m_mutex;
    QList<TaskTrace> m_tasks;
    QString m_currentTaskId;
    int m_taskCounter = 0;
};

} // namespace act::framework
