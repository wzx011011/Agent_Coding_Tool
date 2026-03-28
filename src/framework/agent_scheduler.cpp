#include "framework/agent_scheduler.h"

#include <spdlog/spdlog.h>

namespace act::framework
{

AgentScheduler::AgentScheduler(QObject *parent)
    : QObject(parent)
{
}

void AgentScheduler::loadGraph(const TaskGraph &graph)
{
    m_graph = graph;
    m_waves.clear();
    if (!m_graph.computeWaves(m_waves))
    {
        spdlog::error("AgentScheduler: cycle detected in task graph, cannot schedule");
        m_graph = TaskGraph();
        return;
    }
    m_currentWave = 0;
    spdlog::info("AgentScheduler: loaded graph with {} tasks, {} waves",
                 graph.size(), m_waves.size());
}

void AgentScheduler::setExecutor(TaskExecutor executor)
{
    m_executor = std::move(executor);
}

void AgentScheduler::setProgressCallback(SchedulerProgressCallback callback)
{
    m_progressCallback = std::move(callback);
}

bool AgentScheduler::start()
{
    if (m_state != SchedulerState::Idle)
        return false;

    if (m_graph.size() == 0)
    {
        spdlog::warn("AgentScheduler: empty graph, nothing to execute");
        return false;
    }

    if (m_waves.isEmpty())
    {
        spdlog::error("AgentScheduler: no valid wave schedule (possible cycle)");
        return false;
    }

    m_state = SchedulerState::Running;
    m_currentWave = 0;
    executeNextWave();
    return true;
}

void AgentScheduler::cancel()
{
    if (m_state != SchedulerState::Running)
        return;

    m_state = SchedulerState::Cancelled;
    spdlog::info("AgentScheduler: cancelled");
    emit error(QStringLiteral("Scheduler cancelled"));
}

int AgentScheduler::completedCount() const
{
    return m_graph.tasksByState(TaskNodeState::Completed).size();
}

int AgentScheduler::failedCount() const
{
    return m_graph.tasksByState(TaskNodeState::Failed).size();
}

void AgentScheduler::executeNextWave()
{
    if (m_state != SchedulerState::Running)
        return;

    if (m_currentWave >= m_waves.size())
    {
        m_state = m_graph.allCompleted()
                      ? SchedulerState::Completed
                      : SchedulerState::Failed;
        spdlog::info("AgentScheduler: finished (state={})",
                     static_cast<int>(m_state));
        emit allCompleted();
        return;
    }

    const auto &wave = m_waves[m_currentWave];
    spdlog::info("AgentScheduler: executing wave {} ({} tasks)",
                 m_currentWave + 1, wave.size());

    for (const auto &taskId : wave)
    {
        if (m_state != SchedulerState::Running)
            break;

        auto *task = m_graph.findTaskMutable(taskId);
        if (!task)
        {
            spdlog::warn("AgentScheduler: task '{}' not found", taskId.toStdString());
            continue;
        }

        task->state = TaskNodeState::Running;

        QString result;
        if (m_executor)
        {
            result = m_executor(task->id, task->description);
            task->result = result;
            task->state = result.isEmpty() ? TaskNodeState::Failed
                                       : TaskNodeState::Completed;
        }
        else
        {
            // No executor — mark as completed with empty result
            task->state = TaskNodeState::Completed;
        }

        if (m_progressCallback)
            m_progressCallback(taskId, task->state, task->result);

        if (task->state == TaskNodeState::Completed)
            emit taskCompleted(taskId);
        else
            spdlog::warn("AgentScheduler: task '{}' failed", taskId.toStdString());
    }

    ++m_currentWave;
    executeNextWave();
}

} // namespace act::framework
