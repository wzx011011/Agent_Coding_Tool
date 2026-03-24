#include "framework/execution_lane.h"

#include <spdlog/spdlog.h>

namespace act::framework
{

QString ExecutionLaneManager::createLane(const QString &workspaceDir,
                                          const QString &branchName)
{
    QString laneId = QStringLiteral("lane-%1").arg(m_nextLaneId++);

    ExecutionLane lane;
    lane.id = laneId;
    lane.workspaceDir = workspaceDir;
    lane.branchName = branchName;
    lane.isActive = true;

    m_lanes[laneId] = lane;

    spdlog::info("ExecutionLaneManager: created lane '{}' at '{}'",
                 laneId.toStdString(), workspaceDir.toStdString());
    return laneId;
}

bool ExecutionLaneManager::removeLane(const QString &laneId)
{
    if (!m_lanes.remove(laneId))
        return false;

    spdlog::info("ExecutionLaneManager: removed lane '{}'",
                 laneId.toStdString());
    return true;
}

const ExecutionLane *ExecutionLaneManager::lane(
    const QString &laneId) const
{
    auto it = m_lanes.find(laneId);
    return (it != m_lanes.end()) ? &it.value() : nullptr;
}

ExecutionLane *ExecutionLaneManager::mutableLane(const QString &laneId)
{
    auto it = m_lanes.find(laneId);
    return (it != m_lanes.end()) ? &it.value() : nullptr;
}

QStringList ExecutionLaneManager::laneIds() const
{
    return m_lanes.keys();
}

int ExecutionLaneManager::activeCount() const
{
    int count = 0;
    for (const auto &lane : m_lanes)
    {
        if (lane.isActive)
            ++count;
    }
    return count;
}

bool ExecutionLaneManager::assignTask(const QString &laneId,
                                        const TaskNode &task)
{
    auto *l = mutableLane(laneId);
    if (!l)
        return false;

    l->taskGraph.addTask(task);
    spdlog::debug("ExecutionLaneManager: assigned task '{}' to lane '{}'",
                  task.id.toStdString(), laneId.toStdString());
    return true;
}

QString ExecutionLaneManager::workspaceFor(const QString &laneId) const
{
    auto *l = lane(laneId);
    return l ? l->workspaceDir : QString();
}

bool ExecutionLaneManager::hasLane(const QString &laneId) const
{
    return m_lanes.contains(laneId);
}

} // namespace act::framework
