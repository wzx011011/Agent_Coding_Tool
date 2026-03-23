#pragma once

#include <QJsonObject>
#include <QString>
#include <QUuid>

#include "core/enums.h"

namespace act::core
{

struct ToolCall
{
    QString id;
    QString name;
    QJsonObject params;
};

struct ToolResult
{
    bool success = false;
    QString output;
    QString error;
    QString errorCode;
    QJsonObject metadata;

    [[nodiscard]] static ToolResult ok(QString output, QJsonObject metadata = {})
    {
        return {true, std::move(output), {}, {}, std::move(metadata)};
    }

    [[nodiscard]] static ToolResult err(QString errorCode, QString error)
    {
        return {false, {}, std::move(error), std::move(errorCode), {}};
    }
};

struct LLMMessage
{
    MessageRole role = MessageRole::User;
    QString content;
    QString toolCallId;  // only when role == Tool
    ToolCall toolCall;   // only when role == Assistant and has tool call
};

struct PermissionRequest
{
    QString id;
    PermissionLevel level = PermissionLevel::Read;
    QString toolName;
    QString description;
    QJsonObject params;

    PermissionRequest()
        : id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
    }
};

} // namespace act::core
