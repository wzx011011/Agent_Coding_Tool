#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QString>

#include "core/types.h"

namespace act::services
{

/// Converts between ACT internal types and OpenAI-compatible API format.
/// Used for GLM and other OpenAI-format providers.
class OpenAICompatConverter
{
public:
    struct ParsedResponse
    {
        QString text;
        QList<act::core::ToolCall> toolCalls;
        QString finishReason; // "stop", "tool_calls", "length"
    };

    /// Convert internal messages to OpenAI-compatible API request body.
    [[nodiscard]] static QJsonObject toRequest(
        const QList<act::core::LLMMessage> &messages,
        const QString &model,
        const QList<QJsonObject> &toolDefs = {},
        int maxTokens = 4096,
        double temperature = 0.0);

    /// Parse an OpenAI-compatible SSE event line.
    /// The data field should be the raw JSON string from "data: {...}"
    /// Returns false if the line is [DONE].
    [[nodiscard]] static bool parseSseEvent(
        const QString &data,
        ParsedResponse &response);

    /// Accumulate parsed SSE events into a final complete response.
    [[nodiscard]] static ParsedResponse finalize(const ParsedResponse &accumulated);

    /// Convert a tool schema to OpenAI function definition.
    [[nodiscard]] static QJsonObject toolToDefinition(const QString &name,
                                                        const QString &description,
                                                        const QJsonObject &schema);

    /// Build auth headers for OpenAI-compatible API.
    [[nodiscard]] static QMap<QString, QString> authHeaders(const QString &apiKey);

    /// Convert internal messages to OpenAI messages array.
    [[nodiscard]] static QJsonArray buildMessages(
        const QList<act::core::LLMMessage> &messages);

private:
    /// Convert a single message to OpenAI format.
    [[nodiscard]] static QJsonObject messageToOpenAI(const act::core::LLMMessage &msg);
};

} // namespace act::services
