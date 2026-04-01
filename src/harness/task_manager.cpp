#include "harness/task_manager.h"

#include <QDateTime>

#include <spdlog/spdlog.h>

namespace act::harness
{

TaskManager::TaskManager() = default;

QString TaskManager::createTask(const QString &subject,
                                const QString &description,
                                const QString &activeForm)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    TaskItem task;
    task.id = QString::number(m_nextId++);
    task.subject = subject;
    task.description = description;
    task.activeForm = activeForm;
    task.status = QStringLiteral("pending");
    task.createdAt = QDateTime::currentDateTime();
    task.updatedAt = task.createdAt;

    m_tasks.insert(task.id, task);

    spdlog::info("TaskManager: created task '{}' with id={}",
                 subject.toStdString(), task.id.toStdString());

    return task.id;
}

TaskItem TaskManager::getTask(const QString &id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tasks.value(id);
}

bool TaskManager::hasTask(const QString &id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tasks.contains(id);
}

bool TaskManager::updateTask(const QString &id,
                             const QString &status,
                             const QString &description,
                             const QStringList &addBlockedBy,
                             const QString &activeForm)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_tasks.find(id);
    if (it == m_tasks.end())
        return false;

    auto &task = *it;

    if (!status.isEmpty())
        task.status = status;
    if (!description.isEmpty())
        task.description = description;
    if (!addBlockedBy.isEmpty())
    {
        for (const auto &dep : addBlockedBy)
        {
            if (!task.blockedBy.contains(dep))
                task.blockedBy.append(dep);
        }
    }
    if (!activeForm.isEmpty())
        task.activeForm = activeForm;

    task.updatedAt = QDateTime::currentDateTime();

    spdlog::info("TaskManager: updated task id={}", id.toStdString());

    return true;
}

QList<TaskItem> TaskManager::listTasks(const QString &statusFilter) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QList<TaskItem> result;
    for (const auto &task : m_tasks)
    {
        if (statusFilter.isEmpty() || task.status == statusFilter)
            result.append(task);
    }
    return result;
}

bool TaskManager::deleteTask(const QString &id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_tasks.find(id);
    if (it == m_tasks.end())
        return false;

    it->status = QStringLiteral("deleted");
    it->updatedAt = QDateTime::currentDateTime();

    spdlog::info("TaskManager: deleted task id={}", id.toStdString());

    return true;
}

} // namespace act::harness
