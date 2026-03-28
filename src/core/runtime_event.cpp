#include "core/runtime_event.h"

namespace act::core
{

RuntimeEvent RuntimeEvent::streamToken(const QString &token)
{
    return {EventType::StreamToken, {{QStringLiteral("token"), token}}};
}

RuntimeEvent RuntimeEvent::toolCall(const QString &toolName,
                                    const QJsonObject &params)
{
    return {EventType::ToolCallStarted,
            {{QStringLiteral("tool"), toolName},
             {QStringLiteral("params"), params}}};
}

RuntimeEvent RuntimeEvent::toolCallCompleted(
    const QString &toolName,
    bool success,
    const QString &output,
    const QString &errorCode,
    const QString &errorMsg)
{
    QJsonObject data;
    data[QStringLiteral("tool")] = toolName;
    data[QStringLiteral("success")] = success;
    data[QStringLiteral("output")] = output;
    if (!errorCode.isEmpty())
        data[QStringLiteral("error_code")] = errorCode;
    if (!errorMsg.isEmpty())
        data[QStringLiteral("error_message")] = errorMsg;
    return {EventType::ToolCallCompleted, data};
}

RuntimeEvent RuntimeEvent::permissionRequest(const QString &id,
                                             const QString &toolName,
                                             PermissionLevel level)
{
    return {EventType::PermissionRequested,
            {{QStringLiteral("id"), id},
             {QStringLiteral("tool"), toolName},
             {QStringLiteral("level"), static_cast<int>(level)}}};
}

RuntimeEvent RuntimeEvent::permissionResponse(const QString &id, bool approved)
{
    return {EventType::PermissionResolved,
            {{QStringLiteral("id"), id}, {QStringLiteral("approved"), approved}}};
}

RuntimeEvent RuntimeEvent::userInputRequest(const QString &prompt)
{
    return {EventType::UserInputRequested, {{QStringLiteral("prompt"), prompt}}};
}

RuntimeEvent RuntimeEvent::userInputProvided(const QString &response)
{
    return {EventType::UserInputProvided, {{QStringLiteral("response"), response}}};
}

RuntimeEvent RuntimeEvent::taskState(TaskState state, const QString &summary)
{
    QJsonObject data;
    data[QStringLiteral("state")] = static_cast<int>(state);
    if (!summary.isEmpty())
        data[QStringLiteral("summary")] = summary;
    return {EventType::TaskStateChanged, data};
}

RuntimeEvent RuntimeEvent::error(const QString &code, const QString &message)
{
    return {EventType::ErrorOccurred,
            {{QStringLiteral("code"), code}, {QStringLiteral("message"), message}}};
}

} // namespace act::core
