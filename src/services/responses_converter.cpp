#include "services/responses_converter.h"

#include <QJsonDocument>

namespace act::services {

namespace {

QList<act::core::ToolCall> orderedToolCalls(const act::core::LLMMessage &message) {
    QList<act::core::ToolCall> result;

    auto appendIfUnique = [&result](const act::core::ToolCall &call) {
        if (call.name.isEmpty() && call.id.isEmpty())
            return;

        for (const auto &existing : result) {
            if ((!call.id.isEmpty() && existing.id == call.id) ||
                (call.id.isEmpty() && existing.id.isEmpty() && existing.name == call.name)) {
                return;
            }
        }
        result.append(call);
    };

    appendIfUnique(message.toolCall);
    for (const auto &call : message.toolCalls)
        appendIfUnique(call);

    return result;
}

QJsonObject parseArguments(QStringView arguments) {
    const auto raw = arguments.trimmed();
    if (raw.isEmpty())
        return {};

    const auto doc = QJsonDocument::fromJson(raw.toUtf8());
    if (doc.isObject())
        return doc.object();

    return QJsonObject{{QStringLiteral("_raw"), raw.toString()}};
}

void mergeToolCall(QList<act::core::ToolCall> &toolCalls, const act::core::ToolCall &call) {
    if (call.id.isEmpty() && call.name.isEmpty() && call.params.isEmpty())
        return;

    for (auto &existing : toolCalls) {
        if ((!call.id.isEmpty() && existing.id == call.id) ||
            (call.id.isEmpty() && !call.name.isEmpty() && existing.name == call.name)) {
            if (!call.id.isEmpty())
                existing.id = call.id;
            if (!call.name.isEmpty())
                existing.name = call.name;
            if (!call.params.isEmpty())
                existing.params = call.params;
            return;
        }
    }

    toolCalls.append(call);
}

QString extractTextFromMessageContent(const QJsonArray &content) {
    QString text;
    for (const auto &partValue : content) {
        const auto part = partValue.toObject();
        const auto type = part[QStringLiteral("type")].toString();
        if (type == QLatin1String("output_text") || type == QLatin1String("text")) {
            text += part[QStringLiteral("text")].toString();
        }
    }
    return text;
}

QString extractToolOutputText(const QJsonValue &outputValue) {
    if (outputValue.isString())
        return outputValue.toString();

    QString text;
    const auto parts = outputValue.toArray();
    for (const auto &partValue : parts) {
        const auto part = partValue.toObject();
        const auto type = part[QStringLiteral("type")].toString();
        if (type == QLatin1String("input_text") || type == QLatin1String("text")) {
            text += part[QStringLiteral("text")].toString();
        }
    }
    return text;
}

} // namespace

QJsonObject ResponsesConverter::toRequest(const QList<act::core::LLMMessage> &messages, const QString &model,
                                          const QList<QJsonObject> &toolDefs, int maxOutputTokens) {
    QString instructions;
    QJsonObject request;
    request[QStringLiteral("model")] = model;
    request[QStringLiteral("input")] = buildInput(messages, instructions);
    request[QStringLiteral("max_output_tokens")] = maxOutputTokens;
    request[QStringLiteral("stream")] = true;

    if (!instructions.isEmpty())
        request[QStringLiteral("instructions")] = instructions;

    if (!toolDefs.isEmpty()) {
        QJsonArray tools;
        for (const auto &def : toolDefs) {
            tools.append(toolToDefinition(def[QStringLiteral("name")].toString(),
                                          def[QStringLiteral("description")].toString(),
                                          def[QStringLiteral("schema")].toObject()));
        }
        request[QStringLiteral("tools")] = tools;
    }

    return request;
}

bool ResponsesConverter::parseSseEvent(const act::infrastructure::SseEvent &event, ParsedResponse &response) {
    if (event.data.trimmed() == QStringLiteral("[DONE]"))
        return false;

    const auto doc = QJsonDocument::fromJson(event.data.toUtf8());
    if (!doc.isObject())
        return true;

    const auto obj = doc.object();
    const auto type = obj[QStringLiteral("type")].toString(event.eventType);

    if (type == QLatin1String("response.output_text.delta")) {
        response.text = obj[QStringLiteral("delta")].toString();
        return true;
    }

    if (type == QLatin1String("response.function_call_arguments.done")) {
        act::core::ToolCall call;
        call.id = obj[QStringLiteral("call_id")].toString(obj[QStringLiteral("item_id")].toString());
        call.name = obj[QStringLiteral("name")].toString();
        call.params = parseArguments(obj[QStringLiteral("arguments")].toString());
        mergeToolCall(response.toolCalls, call);
        return true;
    }

    if (type == QLatin1String("response.output_item.done")) {
        appendOutputItem(obj[QStringLiteral("item")].toObject(), response, false);
        return true;
    }

    if (type == QLatin1String("response.completed")) {
        response.status = QStringLiteral("completed");
        const auto responseObj = obj[QStringLiteral("response")].toObject();
        if (!responseObj.isEmpty()) {
            const auto parsed = parseResponseBody(QJsonDocument(responseObj).toJson(QJsonDocument::Compact));
            mergeParsedResponse(response, parsed);
        }
        return true;
    }

    if (type == QLatin1String("response.failed") || type == QLatin1String("response.error") ||
        type == QLatin1String("error")) {
        response.status = QStringLiteral("failed");
        const auto errorObj = obj[QStringLiteral("error")].toObject();
        response.errorCode = errorObj[QStringLiteral("code")].toString(type == QLatin1String("response.failed")
                                                                           ? QStringLiteral("RESPONSE_FAILED")
                                                                           : QStringLiteral("RESPONSE_ERROR"));
        response.errorMessage = errorObj[QStringLiteral("message")].toString(obj[QStringLiteral("message")].toString());
    }

    return true;
}

ResponsesConverter::ParsedResponse ResponsesConverter::parseResponseBody(const QByteArray &responseBody) {
    ParsedResponse response;

    const auto doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject()) {
        response.errorCode = QStringLiteral("PARSE_ERROR");
        response.errorMessage = QStringLiteral("Invalid Responses API JSON");
        return response;
    }

    const auto obj = doc.object();
    response.status = obj[QStringLiteral("status")].toString();

    const auto output = obj[QStringLiteral("output")].toArray();
    for (const auto &itemValue : output)
        appendOutputItem(itemValue.toObject(), response, true);

    return response;
}

ResponsesConverter::ParsedResponse ResponsesConverter::finalize(const ParsedResponse &accumulated) {
    ParsedResponse result = accumulated;
    if (result.status.isEmpty())
        result.status = QStringLiteral("completed");
    return result;
}

QMap<QString, QString> ResponsesConverter::authHeaders(const QString &apiKey) {
    QMap<QString, QString> headers;
    headers[QStringLiteral("Authorization")] = QStringLiteral("Bearer ") + apiKey;
    headers[QStringLiteral("content-type")] = QStringLiteral("application/json");
    return headers;
}

QJsonArray ResponsesConverter::buildInput(const QList<act::core::LLMMessage> &messages, QString &instructions) {
    QJsonArray input;

    for (const auto &message : messages) {
        switch (message.role) {
        case act::core::MessageRole::System:
            if (!message.content.isEmpty()) {
                if (!instructions.isEmpty())
                    instructions += QLatin1Char('\n');
                instructions += message.content;
            }
            break;

        case act::core::MessageRole::User:
            input.append(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("message")},
                {QStringLiteral("role"), QStringLiteral("user")},
                {QStringLiteral("content"), contentParts(message.content, QStringLiteral("user"))},
            });
            break;

        case act::core::MessageRole::Assistant: {
            if (!message.content.isEmpty()) {
                input.append(QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("message")},
                    {QStringLiteral("role"), QStringLiteral("assistant")},
                    {QStringLiteral("content"), contentParts(message.content, QStringLiteral("assistant"))},
                });
            }

            const auto toolCalls = orderedToolCalls(message);
            for (const auto &call : toolCalls) {
                input.append(QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("function_call")},
                    {QStringLiteral("id"), call.id},
                    {QStringLiteral("call_id"), call.id},
                    {QStringLiteral("name"), call.name},
                    {QStringLiteral("arguments"),
                     QString::fromUtf8(QJsonDocument(call.params).toJson(QJsonDocument::Compact))},
                });
            }
            break;
        }

        case act::core::MessageRole::Tool:
            input.append(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("function_call_output")},
                {QStringLiteral("call_id"), message.toolCallId},
                {QStringLiteral("output"), contentParts(message.content, QStringLiteral("user"))},
            });
            break;
        }
    }

    return input;
}

QJsonArray ResponsesConverter::contentParts(QStringView text, QStringView role) {
    const auto contentType =
        role == QStringLiteral("assistant") ? QStringLiteral("output_text") : QStringLiteral("input_text");
    return QJsonArray{QJsonObject{
        {QStringLiteral("type"), contentType},
        {QStringLiteral("text"), text.toString()},
    }};
}

QJsonObject ResponsesConverter::toolToDefinition(const QString &name, const QString &description,
                                                 const QJsonObject &schema) {
    QJsonObject tool;
    tool[QStringLiteral("type")] = QStringLiteral("function");
    tool[QStringLiteral("name")] = name;
    tool[QStringLiteral("description")] = description;

    QJsonObject parameters = schema;
    if (!parameters.contains(QStringLiteral("type")))
        parameters[QStringLiteral("type")] = QStringLiteral("object");
    tool[QStringLiteral("parameters")] = parameters;

    return tool;
}

void ResponsesConverter::mergeParsedResponse(ParsedResponse &target, const ParsedResponse &update) {
    if (!update.text.isEmpty() && target.text.isEmpty())
        target.text = update.text;

    for (const auto &call : update.toolCalls)
        mergeToolCall(target.toolCalls, call);

    if (!update.status.isEmpty())
        target.status = update.status;
    if (!update.errorCode.isEmpty())
        target.errorCode = update.errorCode;
    if (!update.errorMessage.isEmpty())
        target.errorMessage = update.errorMessage;
}

void ResponsesConverter::appendOutputItem(const QJsonObject &item, ParsedResponse &response, bool allowMessageText) {
    const auto type = item[QStringLiteral("type")].toString();

    if (type == QLatin1String("message")) {
        if (allowMessageText)
            response.text += extractTextFromMessageContent(item[QStringLiteral("content")].toArray());
        return;
    }

    if (type == QLatin1String("function_call")) {
        act::core::ToolCall call;
        call.id = item[QStringLiteral("call_id")].toString(item[QStringLiteral("id")].toString());
        call.name = item[QStringLiteral("name")].toString();
        call.params = parseArguments(item[QStringLiteral("arguments")].toString());
        mergeToolCall(response.toolCalls, call);
        return;
    }

    if (type == QLatin1String("function_call_output") && allowMessageText) {
        const auto outputText = extractToolOutputText(item[QStringLiteral("output")]);
        if (!outputText.isEmpty() && response.text.isEmpty())
            response.text = outputText;
    }
}

} // namespace act::services