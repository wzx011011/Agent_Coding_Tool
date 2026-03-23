#include "framework/runtime_trace_store.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace act::framework
{

QString RuntimeTraceStore::beginTask(const QString &description)
{
    TaskTrace trace;
    trace.id = generateTaskId();
    trace.description = description;
    m_tasks.append(std::move(trace));
    m_currentTaskId = m_tasks.last().id;
    return m_currentTaskId;
}

void RuntimeTraceStore::endTask()
{
    m_currentTaskId.clear();
}

void RuntimeTraceStore::record(const act::core::RuntimeEvent &event)
{
    if (m_currentTaskId.isEmpty())
        return;

    for (auto &task : m_tasks)
    {
        if (task.id == m_currentTaskId)
        {
            task.events.append(event);
            return;
        }
    }
}

QList<act::core::RuntimeEvent> RuntimeTraceStore::events(
    const QString &taskId) const
{
    for (const auto &task : m_tasks)
    {
        if (task.id == taskId)
            return task.events;
    }
    return {};
}

QJsonObject RuntimeTraceStore::traceJson(const QString &taskId) const
{
    for (const auto &task : m_tasks)
    {
        if (task.id == taskId)
        {
            QJsonObject root;
            root[QStringLiteral("taskId")] = task.id;
            root[QStringLiteral("description")] = task.description;
            root[QStringLiteral("eventCount")] = task.events.size();

            QJsonArray eventsArr;
            for (const auto &evt : task.events)
            {
                QJsonObject evtObj;
                evtObj[QStringLiteral("type")] =
                    static_cast<int>(evt.type);
                evtObj[QStringLiteral("data")] = evt.data;
                eventsArr.append(evtObj);
            }
            root[QStringLiteral("events")] = eventsArr;
            return root;
        }
    }
    return {};
}

QString RuntimeTraceStore::currentTaskId() const
{
    return m_currentTaskId;
}

int RuntimeTraceStore::taskCount() const
{
    return m_tasks.size();
}

QString RuntimeTraceStore::generateTaskId()
{
    return QStringLiteral("task_%1").arg(++m_taskCounter);
}

} // namespace act::framework
