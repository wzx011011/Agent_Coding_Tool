#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <mutex>

namespace act::harness
{

struct CheckpointEntry
{
    QString id;           // UUID
    QString filePath;     // Original file path (relative to workspace)
    QString backupPath;   // Path to backup copy
    QDateTime timestamp;
    QString operation;    // "edit", "write", "delete"
};

class FileCheckpoint
{
public:
    explicit FileCheckpoint(const QString &workspaceRoot);

    /// Create a checkpoint of the given file content before modification.
    /// Returns the checkpoint ID.
    QString checkpoint(const QString &filePath, const QByteArray &content);

    /// Restore file from a checkpoint. Returns true on success.
    bool restore(const QString &checkpointId);

    /// List all checkpoints, optionally filtered by file path prefix.
    QList<CheckpointEntry> listCheckpoints(const QString &filePathFilter = {}) const;

    /// Remove old checkpoints, keeping at most maxCount most recent.
    /// Returns the number of checkpoints removed.
    int cleanup(int maxCount = 50);

    /// Read the backup content for a given checkpoint ID.
    QByteArray checkpointContent(const QString &checkpointId) const;

private:
    QString checkpointDir() const;
    bool ensureCheckpointDir() const;

    QString m_workspaceRoot;
    mutable std::mutex m_mutex;
    QList<CheckpointEntry> m_index; // In-memory index
};

} // namespace act::harness
