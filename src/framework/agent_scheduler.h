#pragma once

#include <QObject>
#include <functional>

#include "framework/task_graph.h"

namespace act::framework
{

/// Callback type for executing a single task.
using TaskExecutor = std::function<QString(
    const QString &taskId, const QString &description)>;

/// Callback type for reporting scheduler progress.
using SchedulerProgressCallback = std::function<void(
    const QString &taskId, TaskNodeState state, const QString &result)>;

/// Executes a TaskGraph in serial wave order.
/// Each wave contains independent tasks that are executed sequentially
/// within the wave (future parallel support in P4).
class AgentScheduler : public QObject
{
    Q_OBJECT
public:
    explicit AgentScheduler(QObject *parent = nullptr);

    /// Load a task graph to execute.
    void loadGraph(const TaskGraph &graph);

    /// Set the executor callback for running individual tasks.
    void setExecutor(TaskExecutor executor);

    /// Set progress callback.
    void setProgressCallback(SchedulerProgressCallback callback);

    /// Start executing the graph.
    /// Returns true if execution started, false if already running or empty.
    [[nodiscard]] bool start();

    /// Cancel the current execution.
    void cancel();

    /// Current state.
    enum class SchedulerState
    {
        Idle,
        Running,
        Completed,
        Failed,
        Cancelled
    };
    [[nodiscard]] SchedulerState state() const { return m_state; }

    /// Number of tasks completed.
    [[nodiscard]] int completedCount() const;

    /// Number of tasks failed.
    [[nodiscard]] int failedCount() const;

signals:
    /// Emitted when a task completes.
    void taskCompleted(const QString &taskId);

    /// Emitted when all tasks are done.
    void allCompleted();

    /// Emitted when the scheduler encounters an error.
    void error(const QString &message);

private:
    void executeNextWave();

    SchedulerState m_state = SchedulerState::Idle;
    TaskGraph m_graph;
    TaskWave m_waves;
    int m_currentWave = 0;
    TaskExecutor m_executor;
    SchedulerProgressCallback m_progressCallback;
};

} // namespace act::framework
