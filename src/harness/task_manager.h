#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include <mutex>

namespace act::harness
{

struct TaskItem
{
    QString id;
    QString subject;
    QString description;
    QString status;       // "pending", "in_progress", "completed", "deleted"
    QString owner;        // Agent ID or empty
    QStringList blockedBy; // Task IDs this task depends on
    QString activeForm;   // Present continuous form for spinner (e.g., "Running tests")
    QJsonObject metadata; // Arbitrary extension data
    QDateTime createdAt;
    QDateTime updatedAt;
};

class TaskManager
{
public:
    TaskManager();

    /// Create a new task. Returns the generated task ID.
    QString createTask(const QString &subject,
                       const QString &description = {},
                       const QString &activeForm = {});

    /// Retrieve a task by ID. Throws nothing; returns default TaskItem if not found.
    /// Use hasTask() to check existence first.
    [[nodiscard]] TaskItem getTask(const QString &id) const;

    /// Check whether a task with the given ID exists.
    [[nodiscard]] bool hasTask(const QString &id) const;

    /// Update fields of an existing task. Only non-empty parameters are applied.
    /// Returns true if the task was found and updated.
    bool updateTask(const QString &id,
                    const QString &status = {},
                    const QString &description = {},
                    const QStringList &addBlockedBy = {},
                    const QString &activeForm = {});

    /// List tasks, optionally filtered by status.
    [[nodiscard]] QList<TaskItem> listTasks(const QString &statusFilter = {}) const;

    /// Soft-delete a task (sets status to "deleted"). Returns true if found.
    bool deleteTask(const QString &id);

private:
    mutable std::mutex m_mutex;
    QMap<QString, TaskItem> m_tasks;
    int m_nextId = 1;
};

} // namespace act::harness
