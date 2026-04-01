#include "harness/cron_manager.h"

#include <QRegularExpression>
#include <QTime>

#include <spdlog/spdlog.h>

namespace act::harness {

QString CronManager::createCron(const QString &cron,
                                const QString &prompt,
                                bool recurring)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    CronJob job;
    job.id = QStringLiteral("cron_%1").arg(++m_nextId);
    job.cron = cron;
    job.prompt = prompt;
    job.recurring = recurring;
    job.fired = false;
    job.createdAt = QDateTime::currentDateTime();

    m_jobs[job.id] = job;

    spdlog::info("CronManager: created {} cron job '{}' with cron='{}'",
                 recurring ? "recurring" : "one-shot",
                 job.id.toStdString(), cron.toStdString());

    return job.id;
}

void CronManager::deleteCron(const QString &id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_jobs.find(id);
    if (it != m_jobs.end())
    {
        spdlog::info("CronManager: deleted cron job '{}'",
                     id.toStdString());
        m_jobs.erase(it);
    }
}

QList<CronJob> CronManager::listCrons() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QList<CronJob> result;
    result.reserve(static_cast<int>(m_jobs.size()));
    for (const auto &[id, job] : m_jobs)
    {
        result.append(job);
    }
    return result;
}

QList<QString> CronManager::checkDueCrons() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QList<QString> due;
    for (const auto &[id, job] : m_jobs)
    {
        // One-shot jobs: only due if not yet fired
        if (!job.recurring && !job.fired)
        {
            due.append(id);
        }
        // Recurring jobs: always considered due (simplified)
        else if (job.recurring)
        {
            due.append(id);
        }
    }
    return due;
}

bool CronManager::markFired(const QString &id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_jobs.find(id);
    if (it == m_jobs.end())
        return false;

    it->second.fired = true;
    spdlog::info("CronManager: marked cron job '{}' as fired",
                 id.toStdString());
    return true;
}

CronJob CronManager::getCron(const QString &id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_jobs.find(id);
    if (it != m_jobs.end())
        return it->second;
    return {};
}

qint64 CronManager::parseDurationToMinutes(const QString &duration)
{
    if (duration.isEmpty())
        return 0;

    QString d = duration.trimmed().toLower();

    // Match patterns like "2h", "2 hours", "2 hour"
    {
        QRegularExpression re(
            QStringLiteral(R"(^(\d+)\s*h(?:ours?)?$)"));
        auto match = re.match(d);
        if (match.hasMatch())
        {
            return match.captured(1).toLongLong() * 60;
        }
    }

    // Match patterns like "5m", "5 min", "5 minutes"
    {
        QRegularExpression re(
            QStringLiteral(R"(^(\d+)\s*m(?:in(?:utes?)?)?$)"));
        auto match = re.match(d);
        if (match.hasMatch())
        {
            return match.captured(1).toLongLong();
        }
    }

    // Match patterns like "30s", "30 seconds"
    {
        QRegularExpression re(
            QStringLiteral(R"(^(\d+)\s*s(?:ec(?:onds?)?)?$)"));
        auto match = re.match(d);
        if (match.hasMatch())
        {
            return match.captured(1).toLongLong() / 60;
        }
    }

    // Match patterns like "1d", "1 day", "2 days"
    {
        QRegularExpression re(
            QStringLiteral(R"(^(\d+)\s*d(?:ays?)?$)"));
        auto match = re.match(d);
        if (match.hasMatch())
        {
            return match.captured(1).toLongLong() * 24 * 60;
        }
    }

    // Bare number treated as minutes
    {
        QRegularExpression re(QStringLiteral(R"(^(\d+)$)"));
        auto match = re.match(d);
        if (match.hasMatch())
        {
            return match.captured(1).toLongLong();
        }
    }

    return 0;
}

QDateTime CronManager::parseAbsoluteTime(const QString &timeStr)
{
    if (timeStr.isEmpty())
        return {};

    QString lower = timeStr.trimmed().toLower();
    QDateTime now = QDateTime::currentDateTime();

    if (lower == QStringLiteral("today"))
        return now;

    if (lower == QStringLiteral("tomorrow"))
        return now.addDays(1);

    // Try "HH:mm" or "HH:mmAP" format
    {
        // 12-hour with am/pm: "2:30pm", "9am"
        QRegularExpression re(
            QStringLiteral(R"(^(\d{1,2})(?::(\d{2}))?\s*(am|pm)$)"));
        auto match = re.match(lower);
        if (match.hasMatch())
        {
            int hour = match.captured(1).toInt();
            int minute = match.captured(2).isEmpty()
                             ? 0
                             : match.captured(2).toInt();
            QString ampm = match.captured(3);

            if (ampm == QStringLiteral("pm") && hour < 12)
                hour += 12;
            else if (ampm == QStringLiteral("am") && hour == 12)
                hour = 0;

            QTime time(hour, minute);
            if (time.isValid())
            {
                QDateTime target(now.date(), time);
                if (target <= now)
                    target = target.addDays(1);
                return target;
            }
        }
    }

    // Try 24-hour format: "14:30", "9:00"
    {
        QTime time = QTime::fromString(lower, QStringLiteral("H:mm"));
        if (!time.isValid())
            time = QTime::fromString(lower, QStringLiteral("HH:mm"));
        if (time.isValid())
        {
            QDateTime target(now.date(), time);
            if (target <= now)
                target = target.addDays(1);
            return target;
        }
    }

    return {};
}

} // namespace act::harness
