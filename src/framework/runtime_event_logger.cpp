#include "framework/runtime_event_logger.h"

#include <QDateTime>
#include <QJsonDocument>

#include <spdlog/spdlog.h>

namespace act::framework
{

RuntimeEventLogger::RuntimeEventLogger(QString logFilePath)
    : m_logFilePath(std::move(logFilePath))
{
}

void RuntimeEventLogger::log(const act::core::RuntimeEvent &event)
{
    if (!m_enabled)
        return;

    QJsonObject entry;
    entry[QStringLiteral("timestamp")] =
        QDateTime::currentDateTime().toString(Qt::ISODate);

    switch (event.type)
    {
    case act::core::EventType::StreamToken:
        entry[QStringLiteral("type")] = QStringLiteral("stream_token");
        break;
    case act::core::EventType::ToolCallStarted:
        entry[QStringLiteral("type")] = QStringLiteral("tool_call_started");
        break;
    case act::core::EventType::PermissionRequested:
        entry[QStringLiteral("type")] = QStringLiteral("permission_requested");
        break;
    case act::core::EventType::PermissionResolved:
        entry[QStringLiteral("type")] = QStringLiteral("permission_resolved");
        break;
    case act::core::EventType::TaskStateChanged:
        entry[QStringLiteral("type")] = QStringLiteral("task_state_changed");
        break;
    case act::core::EventType::ToolExecutionProgress:
        entry[QStringLiteral("type")] = QStringLiteral("tool_execution_progress");
        break;
    case act::core::EventType::ErrorOccurred:
        entry[QStringLiteral("type")] = QStringLiteral("error_occurred");
        break;
    }

    entry[QStringLiteral("data")] = event.data;
    spdlog::info("[event] {}",
                 QJsonDocument(entry).toJson(QJsonDocument::Compact).toStdString());
}

void RuntimeEventLogger::setLogFilePath(const QString &path)
{
    m_logFilePath = path;
}

QString RuntimeEventLogger::logFilePath() const
{
    return m_logFilePath;
}

void RuntimeEventLogger::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool RuntimeEventLogger::isEnabled() const
{
    return m_enabled;
}

} // namespace act::framework
