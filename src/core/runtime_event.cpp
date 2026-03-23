#include "core/runtime_event.h"

namespace act::core
{

RuntimeEvent RuntimeEvent::streamToken(const QString &token)
{
    return {EventType::StreamToken, {{"token", token}}};
}

RuntimeEvent RuntimeEvent::toolCall(const QString &toolName,
                                    const QJsonObject &params)
{
    return {EventType::ToolCallStarted, {{"tool", toolName}, {"params", params}}};
}

RuntimeEvent RuntimeEvent::permissionRequest(const QString &id,
                                             const QString &toolName,
                                             PermissionLevel level)
{
    return {EventType::PermissionRequested,
            {{"id", id},
             {"tool", toolName},
             {"level", static_cast<int>(level)}}};
}

RuntimeEvent RuntimeEvent::permissionResponse(const QString &id, bool approved)
{
    return {EventType::PermissionResolved,
            {{"id", id}, {"approved", approved}}};
}

RuntimeEvent RuntimeEvent::taskState(TaskState state, const QString &summary)
{
    QJsonObject data;
    data["state"] = static_cast<int>(state);
    if (!summary.isEmpty())
        data["summary"] = summary;
    return {EventType::TaskStateChanged, data};
}

RuntimeEvent RuntimeEvent::error(const QString &code, const QString &message)
{
    return {EventType::ErrorOccurred, {{"code", code}, {"message", message}}};
}

} // namespace act::core
