#pragma once

#include <QJsonObject>
#include <QString>

#include "core/enums.h"

namespace act::core
{

enum class EventType
{
    StreamToken,
    ToolCallStarted,
    ToolCallCompleted,
    PermissionRequested,
    PermissionResolved,
    TaskStateChanged,
    ToolExecutionProgress,
    ErrorOccurred
};

struct RuntimeEvent
{
    EventType type;
    QJsonObject data;

    [[nodiscard]] static RuntimeEvent streamToken(const QString &token);
    [[nodiscard]] static RuntimeEvent toolCall(const QString &toolName,
                                               const QJsonObject &params);
    [[nodiscard]] static RuntimeEvent toolCallCompleted(
        const QString &toolName,
        bool success,
        const QString &output,
        const QString &errorCode = {},
        const QString &errorMsg = {});
    [[nodiscard]] static RuntimeEvent permissionRequest(const QString &id,
                                                       const QString &toolName,
                                                       PermissionLevel level);
    [[nodiscard]] static RuntimeEvent permissionResponse(const QString &id,
                                                        bool approved);
    [[nodiscard]] static RuntimeEvent taskState(TaskState state,
                                                const QString &summary = {});
    [[nodiscard]] static RuntimeEvent error(const QString &code,
                                            const QString &message);
};

} // namespace act::core
