#pragma once

#include "core/runtime_event.h"

namespace act::framework
{

/// Structured event logger based on spdlog.
/// Logs RuntimeEvent instances as structured JSON to a spdlog sink.
class RuntimeEventLogger
{
public:
    RuntimeEventLogger() = default;

    /// Log a runtime event.
    void log(const act::core::RuntimeEvent &event);

    /// Enable or disable logging.
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

private:
    bool m_enabled = true;
};

} // namespace act::framework
