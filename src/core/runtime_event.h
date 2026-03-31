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
    UserInputRequested,
    UserInputProvided,
    TaskStateChanged,
    ToolExecutionProgress,
    ErrorOccurred,
    ModelRequest,
    PermissionAudit
};

enum class PermissionAuditResult
{
    Approved,
    Denied,
    AutoApproved
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
    [[nodiscard]] static RuntimeEvent userInputRequest(const QString &prompt);
    [[nodiscard]] static RuntimeEvent userInputProvided(const QString &response);
    [[nodiscard]] static RuntimeEvent taskState(TaskState state,
                                                const QString &summary = {});
    [[nodiscard]] static RuntimeEvent error(const QString &code,
                                            const QString &message);
    [[nodiscard]] static RuntimeEvent modelRequest(const QString &model,
                                                   int inputTokens,
                                                   int outputTokens,
                                                   int latencyMs,
                                                   int turn);
    [[nodiscard]] static RuntimeEvent permissionAudit(
        const QString &toolName,
        const QString &level,
        PermissionAuditResult result,
        const QString &reason = {});
};

} // namespace act::core
