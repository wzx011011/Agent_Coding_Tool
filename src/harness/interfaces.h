#pragma once

#include <QJsonObject>
#include <QString>

#include "core/types.h"

namespace act::harness
{

class ITool
{
public:
    virtual ~ITool() = default;

    // Metadata — consumed by LLM for tool call generation
    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual QString description() const = 0;
    [[nodiscard]] virtual QJsonObject schema() const = 0;

    // Execution
    virtual act::core::ToolResult execute(const QJsonObject &params) = 0;

    // Permission
    [[nodiscard]] virtual act::core::PermissionLevel permissionLevel() const = 0;

    // Thread safety
    [[nodiscard]] virtual bool isThreadSafe() const { return false; }
};

} // namespace act::harness
