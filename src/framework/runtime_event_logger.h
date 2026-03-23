#pragma once

#include <QObject>

#include <QString>

#include "core/runtime_event.h"

namespace act::framework
{

/// Structured event logger based on spdlog.
/// Logs RuntimeEvent instances as structured JSON to a spdlog sink.
class RuntimeEventLogger
{
public:
    explicit RuntimeEventLogger(QString logFilePath = {});

    /// Log a runtime event.
    void log(const act::core::RuntimeEvent &event);

    /// Set the log file path.
    void setLogFilePath(const QString &path);

    /// Get the current log file path.
    [[nodiscard]] QString logFilePath() const;

    /// Enable or disable logging.
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

private:
    QString m_logFilePath;
    bool m_enabled = true;
};

} // namespace act::framework
