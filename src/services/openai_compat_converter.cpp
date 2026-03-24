#include "services/openai_compat_converter.h"

namespace act::services
{

QJsonObject OpenAICompatConverter::toRequest(
    const QList<act::core::LLMMessage> &messages,
    const QString &model,
    const QList<QJsonObject> &toolDefs,
    int maxTokens,
    double temperature)
{
    QJsonObject request;
    request[QStringLiteral("model")] = model;
    request[QStringLiteral("messages")] = buildMessages(messages);
    request[QStringLiteral("max_tokens")] = maxTokens;
    request[QStringLiteral("temperature")] = temperature;
    request[QStringLiteral("stream")] = true;

    // Add tools as OpenAI function definitions - convert to OpenAI format
    if (!toolDefs.isEmpty())
    {
        QJsonArray tools;
        for (const auto &def : toolDefs)
        {
            QString name = def[QStringLiteral("name")].toString();
            QString description = def[QStringLiteral("description")].toString();
            QJsonObject schema = def[QStringLiteral("schema")].toObject();

            // Use toolToDefinition to convert to OpenAI format
            tools.append(toolToDefinition(name, description, schema));
        }
        request[QStringLiteral("tools")] = tools;
    }

    return request;
}

QJsonArray OpenAICompatConverter::buildMessages(
    const QList<act::core::LLMMessage> &messages)
{
    QJsonArray result;

    for (const auto &msg : messages)
    {
        result.append(messageToOpenAI(msg));
    }

    return result;
}

QJsonObject OpenAICompatConverter::messageToOpenAI(const act::core::LLMMessage &msg)
{
    QJsonObject jsonMsg;

    if (msg.role == act::core::MessageRole::System)
    {
        jsonMsg[QStringLiteral("role")] = QStringLiteral("system");
        jsonMsg[QStringLiteral("content")] = msg.content;
    }
    else if (msg.role == act::core::MessageRole::User)
    {
        jsonMsg[QStringLiteral("role")] = QStringLiteral("user");
        jsonMsg[QStringLiteral("content")] = msg.content;
    }
    else if (msg.role == act::core::MessageRole::Assistant)
    {
        jsonMsg[QStringLiteral("role")] = QStringLiteral("assistant");

        if (!msg.content.isEmpty())
            jsonMsg[QStringLiteral("content")] = msg.content;

        // Build tool_calls array
        QJsonArray toolCalls;
        if (!msg.toolCall.id.isEmpty() && !msg.toolCall.name.isEmpty())
        {
            QJsonObject tc;
            tc[QStringLiteral("id")] = msg.toolCall.id;
            tc[QStringLiteral("type")] = QStringLiteral("function");
            QJsonObject fn;
            fn[QStringLiteral("name")] = msg.toolCall.name;
            fn[QStringLiteral("arguments")] = QString::fromUtf8(
                QJsonDocument(msg.toolCall.params).toJson(QJsonDocument::Compact));
            tc[QStringLiteral("function")] = fn;
            toolCalls.append(tc);
        }
        for (const auto &call : msg.toolCalls)
        {
            if (call.id.isEmpty() || call.name.isEmpty())
                continue;
            if (call.id == msg.toolCall.id)
                continue;
            QJsonObject tc;
            tc[QStringLiteral("id")] = call.id;
            tc[QStringLiteral("type")] = QStringLiteral("function");
            QJsonObject fn;
            fn[QStringLiteral("name")] = call.name;
            fn[QStringLiteral("arguments")] = QString::fromUtf8(
                QJsonDocument(call.params).toJson(QJsonDocument::Compact));
            tc[QStringLiteral("function")] = fn;
            toolCalls.append(tc);
        }

        if (!toolCalls.isEmpty())
            jsonMsg[QStringLiteral("tool_calls")] = toolCalls;
    }
    else if (msg.role == act::core::MessageRole::Tool)
    {
        jsonMsg[QStringLiteral("role")] = QStringLiteral("tool");
        jsonMsg[QStringLiteral("tool_call_id")] = msg.toolCallId;
        jsonMsg[QStringLiteral("content")] = msg.content;
    }

    return jsonMsg;
}

bool OpenAICompatConverter::parseSseEvent(
    const QString &data,
    ParsedResponse &response)
{
    if (data.trimmed() == QStringLiteral("[DONE]"))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (!doc.isObject())
        return true; // skip invalid JSON

    QJsonObject obj = doc.object();
    auto choices = obj[QStringLiteral("choices")].toArray();
    if (choices.isEmpty())
        return true;

    auto choice = choices[0].toObject();
    auto delta = choice[QStringLiteral("delta")].toObject();
    response.finishReason =
        choice[QStringLiteral("finish_reason")].toString();

    // Text delta
    if (delta.contains(QStringLiteral("content")))
    {
        response.text = delta[QStringLiteral("content")].toString();
    }

    // Tool call delta
    if (delta.contains(QStringLiteral("tool_calls")))
    {
        auto tcs = delta[QStringLiteral("tool_calls")].toArray();
        for (const auto &tcVal : tcs)
        {
            auto tc = tcVal.toObject();
            int index = tc[QStringLiteral("index")].toInt(-1);
            auto fn = tc[QStringLiteral("function")].toObject();

            // Ensure we have enough tool calls
            while (response.toolCalls.size() <= index)
            {
                act::core::ToolCall call;
                response.toolCalls.append(call);
            }

            if (fn.contains(QStringLiteral("name")))
                response.toolCalls[index].name = fn[QStringLiteral("name")].toString();
            if (fn.contains(QStringLiteral("arguments")))
            {
                // Append partial JSON
                QString args = fn[QStringLiteral("arguments")].toString();
                // Accumulate: store in a special way
                if (!args.isEmpty())
                {
                    QString existing = response.toolCalls[index].params.isEmpty()
                        ? QString()
                        : QString::fromUtf8(
                            QJsonDocument(response.toolCalls[index].params)
                                .toJson(QJsonDocument::Compact));
                    existing += args;
                    QJsonDocument argDoc = QJsonDocument::fromJson(existing.toUtf8());
                    if (argDoc.isObject())
                        response.toolCalls[index].params = argDoc.object();
                    else
                        response.toolCalls[index].params[QStringLiteral("_partial")] = existing;
                }
            }
            if (tc.contains(QStringLiteral("id")))
                response.toolCalls[index].id = tc[QStringLiteral("id")].toString();
        }
    }

    return true;
}

OpenAICompatConverter::ParsedResponse OpenAICompatConverter::finalize(
    const ParsedResponse &accumulated)
{
    ParsedResponse result = accumulated;
    if (result.finishReason.isEmpty())
        result.finishReason = QStringLiteral("stop");
    return result;
}

QJsonObject OpenAICompatConverter::toolToDefinition(
    const QString &name,
    const QString &description,
    const QJsonObject &schema)
{
    QJsonObject tool;
    tool[QStringLiteral("type")] = QStringLiteral("function");

    QJsonObject fn;
    fn[QStringLiteral("name")] = name;
    fn[QStringLiteral("description")] = description;

    // Build parameters (JSON Schema)
    QJsonObject parameters;
    parameters[QStringLiteral("type")] = QStringLiteral("object");

    if (schema.contains(QStringLiteral("properties")))
        parameters[QStringLiteral("properties")] = schema[QStringLiteral("properties")];
    if (schema.contains(QStringLiteral("required")))
        parameters[QStringLiteral("required")] = schema[QStringLiteral("required")];

    fn[QStringLiteral("parameters")] = parameters;
    tool[QStringLiteral("function")] = fn;

    return tool;
}

QMap<QString, QString> OpenAICompatConverter::authHeaders(const QString &apiKey)
{
    QMap<QString, QString> headers;
    headers[QStringLiteral("Authorization")] = QStringLiteral("Bearer ") + apiKey;
    headers[QStringLiteral("content-type")] = QStringLiteral("application/json");
    return headers;
}

} // namespace act::services
