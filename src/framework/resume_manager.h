#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>

#include "framework/agent_loop.h"

namespace act::framework
{

/// Manages task checkpoints for resume/recovery functionality.
/// Stores checkpoint snapshots keyed by task ID, allowing tasks
/// to be resumed from their last confirmed state after interruption.
class ResumeManager
{
public:
    /// Save a checkpoint for a task.
    void saveCheckpoint(const QString &taskId,
                         const AgentLoop::Checkpoint &cp);

    /// Load the checkpoint for a task.
    /// Returns std::nullopt if no checkpoint exists.
    [[nodiscard]] std::optional<AgentLoop::Checkpoint> loadCheckpoint(
        const QString &taskId) const;

    /// Check if a checkpoint exists for a task.
    [[nodiscard]] bool hasCheckpoint(const QString &taskId) const;

    /// Remove the checkpoint for a task.
    void removeCheckpoint(const QString &taskId);

    /// Clear all checkpoints.
    void clearAll();

    /// List all task IDs that have checkpoints.
    [[nodiscard]] QStringList savedTaskIds() const;

    /// Serialize all checkpoints to JSON for persistence.
    [[nodiscard]] QJsonObject serialize() const;

    /// Restore checkpoints from JSON.
    void deserialize(const QJsonObject &json);

private:
    /// Serialize a single checkpoint to JSON.
    [[nodiscard]] static QJsonObject checkpointToJson(
        const AgentLoop::Checkpoint &cp);

    /// Deserialize a single checkpoint from JSON.
    [[nodiscard]] static std::optional<AgentLoop::Checkpoint> checkpointFromJson(
        const QJsonObject &json);

    QMap<QString, AgentLoop::Checkpoint> m_checkpoints;
};

} // namespace act::framework
