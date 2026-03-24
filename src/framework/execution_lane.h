#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include "framework/task_graph.h"

namespace act::framework
{

/// Represents an isolated execution context for a task or group of tasks.
/// v1: Manages workspace directory mapping without actual git worktree
/// operations (future v2 will add real git worktree support).
struct ExecutionLane
{
    QString id;              // Lane identifier
    QString workspaceDir;    // Working directory for this lane
    QString branchName;      // Git branch (if applicable)
    TaskGraph taskGraph;     // Tasks assigned to this lane
    bool isActive = false;
};

/// Manages execution lanes that provide task isolation.
/// v1: Directory-based isolation without real git worktree support.
class ExecutionLaneManager
{
public:
    /// Create a new execution lane.
    /// Returns the lane ID.
    [[nodiscard]] QString createLane(const QString &workspaceDir,
                                       const QString &branchName = {});

    /// Remove an execution lane by ID.
    bool removeLane(const QString &laneId);

    /// Get a lane by ID.
    [[nodiscard]] const ExecutionLane *lane(
        const QString &laneId) const;

    /// Get mutable lane by ID.
    [[nodiscard]] ExecutionLane *mutableLane(const QString &laneId);

    /// List all lane IDs.
    [[nodiscard]] QStringList laneIds() const;

    /// Number of active lanes.
    [[nodiscard]] int activeCount() const;

    /// Assign a task to a specific lane.
    bool assignTask(const QString &laneId, const TaskNode &task);

    /// Get the workspace directory for a lane.
    [[nodiscard]] QString workspaceFor(const QString &laneId) const;

    /// Check if a lane exists.
    [[nodiscard]] bool hasLane(const QString &laneId) const;

private:
    QMap<QString, ExecutionLane> m_lanes;
    int m_nextLaneId = 1;
};

} // namespace act::framework
