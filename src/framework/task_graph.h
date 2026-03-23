#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace act::framework
{

/// Task state for the task graph.
enum class TaskNodeState
{
    Pending,    // Not yet started
    Running,    // Currently executing
    Completed,  // Successfully finished
    Failed,     // Failed with error
    Skipped     // Skipped (dependency failed or manually skipped)
};

/// A single node in the task graph.
struct TaskNode
{
    QString id;
    QString title;
    QString description;
    QStringList dependencies; // IDs of tasks this depends on
    TaskNodeState state = TaskNodeState::Pending;
    QString result;          // Summary after completion
    QString error;           // Error message if failed
    QJsonObject metadata;    // Extension data (commit hash, duration, etc.)
};

/// Topologically sorted list of task groups.
/// Each group contains tasks that can run in parallel.
using TaskWave = QList<QStringList>;

/// Manages a directed acyclic graph of tasks with dependency ordering.
class TaskGraph
{
public:
    /// Add a task node to the graph.
    [[nodiscard]] bool addTask(const TaskNode &task);

    /// Update the state of an existing task.
    [[nodiscard]] bool updateState(const QString &id, TaskNodeState state,
                                    const QString &result = {});

    /// Get a task by ID.
    [[nodiscard]] const TaskNode *findTask(const QString &id) const;

    /// Get mutable task by ID.
    [[nodiscard]] TaskNode *findTaskMutable(const QString &id);

    /// List all task IDs.
    [[nodiscard]] QStringList listTasks() const;

    /// Compute topological waves (parallel groups).
    /// Returns false if a cycle is detected.
    [[nodiscard]] bool computeWaves(TaskWave &waves) const;

    /// Get tasks that are ready to execute (all deps completed).
    [[nodiscard]] QStringList readyTasks() const;

    /// Check if all tasks are completed (or skipped).
    [[nodiscard]] bool allCompleted() const;

    /// Number of tasks.
    [[nodiscard]] int size() const;

    /// Get tasks in a specific state.
    [[nodiscard]] QStringList tasksByState(TaskNodeState state) const;

private:
    [[nodiscard]] bool hasCycle() const;
    [[nodiscard]] int inDegree(const QString &id) const;

    QList<TaskNode> m_tasks;
};

/// Persists task state to JSON for resume/replay support.
class TaskStateStore
{
public:
    /// Save the task graph state to a JSON object.
    [[nodiscard]] QJsonObject serialize(const TaskGraph &graph) const;

    /// Restore a task graph from a JSON object.
    [[nodiscard]] TaskGraph deserialize(const QJsonObject &json) const;

    /// Save task graph state to a file.
    [[nodiscard]] bool saveToFile(const TaskGraph &graph,
                                   const QString &filePath) const;

    /// Load task graph state from a file.
    [[nodiscard]] TaskGraph loadFromFile(const QString &filePath) const;
};

} // namespace act::framework
