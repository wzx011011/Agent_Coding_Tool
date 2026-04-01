#include "harness/file_checkpoint.h"

#include <QDir>
#include <QFile>
#include <QUuid>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace act::harness
{

FileCheckpoint::FileCheckpoint(const QString &workspaceRoot)
    : m_workspaceRoot(QDir::cleanPath(workspaceRoot))
{
}

QString FileCheckpoint::checkpoint(const QString &filePath, const QByteArray &content)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!ensureCheckpointDir())
    {
        spdlog::warn("FileCheckpoint: failed to ensure checkpoint directory");
        return {};
    }

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString shortId = id.left(8);
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString dirName = QStringLiteral("%1-%2").arg(ts, shortId);

    // Compute relative path for the backup subdirectory structure
    QString relPath = filePath;
    // Normalize to forward slashes
    relPath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    // Remove leading slash or drive letter
    if (relPath.startsWith(QLatin1Char('/')))
        relPath.remove(0, 1);

    const QString backupDir = checkpointDir() + QLatin1Char('/') + dirName;
    QDir().mkpath(backupDir);

    const QString backupPath = backupDir + QLatin1Char('/') +
        QFileInfo(relPath).fileName();

    // Write backup content
    QFile file(backupPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        spdlog::warn("FileCheckpoint: failed to write backup to {}", backupPath.toStdString());
        return {};
    }
    file.write(content);
    file.close();

    CheckpointEntry entry;
    entry.id = id;
    entry.filePath = filePath;
    entry.backupPath = backupPath;
    entry.timestamp = QDateTime::currentDateTime();
    entry.operation = QStringLiteral("edit");

    m_index.append(std::move(entry));

    spdlog::debug("FileCheckpoint: created checkpoint {} for file {}",
                  id.toStdString(), filePath.toStdString());

    return id;
}

bool FileCheckpoint::restore(const QString &checkpointId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_index.begin(), m_index.end(),
        [&checkpointId](const CheckpointEntry &e) {
            return e.id == checkpointId;
        });

    if (it == m_index.end())
    {
        spdlog::warn("FileCheckpoint: checkpoint {} not found", checkpointId.toStdString());
        return false;
    }

    // Read backup content
    QFile backupFile(it->backupPath);
    if (!backupFile.open(QIODevice::ReadOnly))
    {
        spdlog::warn("FileCheckpoint: cannot read backup at {}", it->backupPath.toStdString());
        return false;
    }
    const QByteArray content = backupFile.readAll();
    backupFile.close();

    // Restore to original path (resolve relative to workspace)
    QString restorePath;
    if (QDir::isAbsolutePath(it->filePath))
    {
        restorePath = it->filePath;
    }
    else
    {
        restorePath = m_workspaceRoot + QLatin1Char('/') + it->filePath;
    }

    QFile targetFile(restorePath);
    if (!targetFile.open(QIODevice::WriteOnly))
    {
        spdlog::warn("FileCheckpoint: cannot restore to {}", restorePath.toStdString());
        return false;
    }
    targetFile.write(content);
    targetFile.close();

    spdlog::info("FileCheckpoint: restored {} to {}", checkpointId.toStdString(), restorePath.toStdString());
    return true;
}

QList<CheckpointEntry> FileCheckpoint::listCheckpoints(const QString &filePathFilter) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (filePathFilter.isEmpty())
    {
        return m_index;
    }

    QList<CheckpointEntry> result;
    for (const auto &entry : m_index)
    {
        if (entry.filePath.contains(filePathFilter))
        {
            result.append(entry);
        }
    }
    return result;
}

int FileCheckpoint::cleanup(int maxCount)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_index.size() <= maxCount)
    {
        return 0;
    }

    // Sort by timestamp descending (most recent first)
    std::sort(m_index.begin(), m_index.end(),
        [](const CheckpointEntry &a, const CheckpointEntry &b) {
            return a.timestamp > b.timestamp;
        });

    int removed = 0;
    while (m_index.size() > static_cast<qsizetype>(maxCount))
    {
        CheckpointEntry entry = m_index.takeLast();

        // Remove backup file
        QFile::remove(entry.backupPath);

        // Try to remove the backup directory if empty
        const QFileInfo fi(entry.backupPath);
        QDir dir = fi.absoluteDir();
        dir.rmdir(QStringLiteral(".")); // only removes if empty

        ++removed;
    }

    spdlog::debug("FileCheckpoint: cleanup removed {} old checkpoints", removed);
    return removed;
}

QByteArray FileCheckpoint::checkpointContent(const QString &checkpointId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_index.begin(), m_index.end(),
        [&checkpointId](const CheckpointEntry &e) {
            return e.id == checkpointId;
        });

    if (it == m_index.end())
    {
        return {};
    }

    QFile file(it->backupPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return file.readAll();
}

QString FileCheckpoint::checkpointDir() const
{
    return m_workspaceRoot + QStringLiteral("/.act/checkpoints");
}

bool FileCheckpoint::ensureCheckpointDir() const
{
    QDir dir(m_workspaceRoot);
    return dir.mkpath(QStringLiteral(".act/checkpoints"));
}

} // namespace act::harness
