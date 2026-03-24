#include "services/anthropic_converter.h"

namespace act::services
{

QJsonObject AnthropicConverter::toRequest(
    const QList<act::core::LLMMessage> &messages,
    const QString &model,
    int maxTokens,
    const QList<QJsonObject> &toolDefs)
{
    QJsonObject request;
    request[QStringLiteral("model")] = model;
    request[QStringLiteral("max_tokens")] = maxTokens;

    // Extract system message
    QString systemPrompt;
    for (const auto &msg : messages)
    {
        if (msg.role == act::core::MessageRole::System && !msg.content.isEmpty())
        {
            if (!systemPrompt.isEmpty())
                systemPrompt += QLatin1Char('\n');
            systemPrompt += msg.content;
        }
    }
    if (!systemPrompt.isEmpty())
        request[QStringLiteral("system")] = systemPrompt;

    // Build non-system messages
    request[QStringLiteral("messages")] = buildMessages(messages);

    // Add tools if any - convert to Anthropic format
    if (!toolDefs.isEmpty())
    {
        QJsonArray tools;
        for (const auto &def : toolDefs)
        {
            QString name = def[QStringLiteral("name")].toString();
            QString description = def[QStringLiteral("description")].toString();
            QJsonObject schema = def[QStringLiteral("schema")].toObject();

            // Use toolToDefinition to convert to Anthropic format
            tools.append(toolToDefinition(name, description, schema));
        }
        request[QStringLiteral("tools")] = tools;
    }

    request[QStringLiteral("stream")] = true;
    return request;
}

QJsonArray AnthropicConverter::buildMessages(
    const QList<act::core::LLMMessage> &messages)
{
    QJsonArray result;

    for (const auto &msg : messages)
    {
        if (msg.role == act::core::MessageRole::System)
            continue; // Already extracted to top-level

        result.append(messageToAnthropic(msg));
    }

    return result;
}

QJsonObject AnthropicConverter::messageToAnthropic(const act::core::LLMMessage &msg)
{
    QJsonObject jsonMsg;

    if (msg.role == act::core::MessageRole::User)
    {
        jsonMsg[QStringLiteral("role")] = QStringLiteral("user");
        QJsonArray content;
        QJsonObject textBlock;
        textBlock[QStringLiteral("type")] = QStringLiteral("text");
        textBlock[QStringLiteral("text")] = msg.content;
        content.append(textBlock);
        jsonMsg[QStringLiteral("content")] = content;
    }
    else if (msg.role == act::core::MessageRole::Assistant)
    {
        jsonMsg[QStringLiteral("role")] = QStringLiteral("assistant");
        QJsonArray content;

        // Text content
        if (!msg.content.isEmpty())
        {
            QJsonObject textBlock;
            textBlock[QStringLiteral("type")] = QStringLiteral("text");
            textBlock[QStringLiteral("text")] = msg.content;
            content.append(textBlock);
        }

        // Tool use blocks (single toolCall for backward compat)
        if (!msg.toolCall.id.isEmpty() && !msg.toolCall.name.isEmpty())
        {
            QJsonObject toolBlock;
            toolBlock[QStringLiteral("type")] = QStringLiteral("tool_use");
            toolBlock[QStringLiteral("id")] = msg.toolCall.id;
            toolBlock[QStringLiteral("name")] = msg.toolCall.name;
            toolBlock[QStringLiteral("input")] = msg.toolCall.params;
            content.append(toolBlock);
        }

        // Multiple tool calls
        for (const auto &tc : msg.toolCalls)
        {
            if (tc.id.isEmpty() || tc.name.isEmpty())
                continue;
            // Skip if already included as single toolCall
            if (tc.id == msg.toolCall.id)
                continue;
            QJsonObject toolBlock;
            toolBlock[QStringLiteral("type")] = QStringLiteral("tool_use");
            toolBlock[QStringLiteral("id")] = tc.id;
            toolBlock[QStringLiteral("name")] = tc.name;
            toolBlock[QStringLiteral("input")] = tc.params;
            content.append(toolBlock);
        }

        jsonMsg[QStringLiteral("content")] = content;
    }
    else if (msg.role == act::core::MessageRole::Tool)
    {
        jsonMsg[QStringLiteral("role")] = QStringLiteral("user");
        QJsonArray content;
        QJsonObject toolResultBlock;
        toolResultBlock[QStringLiteral("type")] = QStringLiteral("tool_result");
        toolResultBlock[QStringLiteral("tool_use_id")] = msg.toolCallId;

        bool isError = msg.content.startsWith(QStringLiteral("Error ["));
        QJsonObject contentObj;
        contentObj[QStringLiteral("type")] = isError ? QStringLiteral("text") : QStringLiteral("text");
        contentObj[QStringLiteral("text")] = msg.content;
        toolResultBlock[QStringLiteral("content")] = contentObj;
        if (isError)
            toolResultBlock[QStringLiteral("is_error")] = true;

        content.append(toolResultBlock);
        jsonMsg[QStringLiteral("content")] = content;
    }

    return jsonMsg;
}

AnthropicConverter::ParsedResponse AnthropicConverter::parseSseEvent(
    const QString &eventType,
    const QJsonObject &data)
{
    ParsedResponse response;

    QString type = data.contains(QStringLiteral("type"))
        ? data[QStringLiteral("type")].toString()
        : eventType;

    if (type == QLatin1String("message_start"))
    {
        auto msg = data[QStringLiteral("message")].toObject();
        response.stopReason = msg[QStringLiteral("stop_reason")].toString();
    }
    else if (type == QLatin1String("content_block_start"))
    {
        auto cb = data[QStringLiteral("content_block")].toObject();
        if (cb[QStringLiteral("type")].toString() == QLatin1String("tool_use"))
        {
            act::core::ToolCall tc;
            tc.id = cb[QStringLiteral("id")].toString();
            tc.name = cb[QStringLiteral("name")].toString();
            response.toolCalls.append(tc);
        }
    }
    else if (type == QLatin1String("content_block_delta"))
    {
        auto delta = data[QStringLiteral("delta")].toObject();
        auto deltaType = delta[QStringLiteral("type")].toString();

        if (deltaType == QLatin1String("text_delta"))
        {
            response.text = delta[QStringLiteral("text")].toString();
        }
        else if (deltaType == QLatin1String("input_json_delta"))
        {
            response.text = delta[QStringLiteral("partial_json")].toString();
        }
    }
    else if (type == QLatin1String("content_block_stop"))
    {
        // No action needed — block boundary
    }
    else if (type == QLatin1String("message_delta"))
    {
        auto delta = data[QStringLiteral("delta")].toObject();
        response.stopReason = delta[QStringLiteral("stop_reason")].toString();
    }
    else if (type == QLatin1String("message_stop"))
    {
        response.stopReason = response.stopReason.isEmpty()
            ? QStringLiteral("end_turn")
            : response.stopReason;
    }

    return response;
}

AnthropicConverter::ParsedResponse AnthropicConverter::finalize(
    const ParsedResponse &accumulated)
{
    ParsedResponse result = accumulated;
    if (result.stopReason.isEmpty())
        result.stopReason = QStringLiteral("end_turn");
    return result;
}

QJsonObject AnthropicConverter::toolToDefinition(
    const QString &name,
    const QString &description,
    const QJsonObject &schema)
{
    QJsonObject tool;
    tool[QStringLiteral("name")] = name;
    tool[QStringLiteral("description")] = description;

    // Anthropic uses input_schema (JSON Schema)
    if (schema.contains(QStringLiteral("input_schema")))
    {
        tool[QStringLiteral("input_schema")] = schema[QStringLiteral("input_schema")];
    }
    else
    {
        // Convert ITool::schema() format to Anthropic input_schema
        QJsonObject inputSchema;
        inputSchema[QStringLiteral("type")] = QStringLiteral("object");

        QJsonObject properties;
        QJsonArray required;

        if (schema.contains(QStringLiteral("properties")))
        {
            properties = schema[QStringLiteral("properties")].toObject();
            inputSchema[QStringLiteral("properties")] = properties;
        }
        if (schema.contains(QStringLiteral("required")))
        {
            required = schema[QStringLiteral("required")].toArray();
            inputSchema[QStringLiteral("required")] = required;
        }

        tool[QStringLiteral("input_schema")] = inputSchema;
    }

    return tool;
}

QMap<QString, QString> AnthropicConverter::authHeaders(const QString &apiKey)
{
    QMap<QString, QString> headers;
    headers[QStringLiteral("x-api-key")] = apiKey;
    headers[QStringLiteral("anthropic-version")] = QStringLiteral("2023-06-01");
    headers[QStringLiteral("content-type")] = QStringLiteral("application/json");
    return headers;
}

} // namespace act::services
