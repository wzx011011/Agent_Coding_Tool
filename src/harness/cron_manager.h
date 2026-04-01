#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QString>

#include <mutex>

namespace act::harness {

/// A scheduled cron job (one-shot or recurring).
struct CronJob {
    QString id;
    QString cron;          // Cron expression: "M H DoM Mon DoW"
    QString prompt;        // What to execute when the job fires
    bool recurring = true; // true = repeating, false = one-shot
    bool fired = false;    // true after one-shot fires
    QDateTime createdAt;
};

/// Manages cron-style scheduled tasks.
/// Supports creating, deleting, listing, and checking for due jobs.
/// Thread-safe via std::mutex.
class CronManager {
public:
    CronManager() = default;

    /// Create a new cron job. Returns the generated job ID.
    QString createCron(const QString &cron,
                       const QString &prompt,
                       bool recurring);

    /// Delete a cron job by ID. Returns true if found and removed.
    void deleteCron(const QString &id);

    /// List all cron jobs.
    QList<CronJob> listCrons() const;

    /// Check for jobs that are due to fire.
    /// For one-shot jobs, returns IDs of jobs not yet fired.
    /// For recurring jobs, always returns them (simplified check).
    QList<QString> checkDueCrons() const;

    /// Mark a one-shot job as fired. Returns true if found.
    bool markFired(const QString &id);

    /// Parse a relative duration string to minutes.
    /// Supports: "5m", "10 min", "2h", "1 hour", "30s"
    static qint64 parseDurationToMinutes(const QString &duration);

    /// Parse an absolute time string to QDateTime.
    /// Supports: "today", "tomorrow", "HH:mm", "HH:mmAP"
    static QDateTime parseAbsoluteTime(const QString &timeStr);

    /// Get a cron job by ID. Returns empty CronJob if not found.
    CronJob getCron(const QString &id) const;

private:
    mutable std::mutex m_mutex;
    std::map<QString, CronJob> m_jobs;
    int m_nextId = 0;
};

} // namespace act::harness
