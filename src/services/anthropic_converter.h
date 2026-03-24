#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QString>

#include "core/types.h"

namespace act::services
{

/// Converts between ACT internal types and Anthropic Messages API format.
class AnthropicConverter
{
public:
    struct ParsedResponse
    {
        QString text;
        QList<act::core::ToolCall> toolCalls;
        QString stopReason; // "end_turn", "tool_use", "max_tokens"
    };

    /// Convert internal messages to Anthropic API request body.
    /// Returns { system: QString, messages: QJsonArray }.
    [[nodiscard]] static QJsonObject toRequest(
        const QList<act::core::LLMMessage> &messages,
        const QString &model,
        int maxTokens,
        const QList<QJsonObject> &toolDefs = {});

    /// Parse an Anthropic SSE event into a partial response.
    [[nodiscard]] static ParsedResponse parseSseEvent(const QString &eventType,
                                                        const QJsonObject &data);

    /// Accumulate parsed SSE events into a final complete response.
    [[nodiscard]] static ParsedResponse finalize(const ParsedResponse &accumulated);

    /// Convert a tool schema (from ITool::schema()) to Anthropic tool definition.
    [[nodiscard]] static QJsonObject toolToDefinition(const QString &name,
                                                        const QString &description,
                                                        const QJsonObject &schema);

    /// Build auth headers for Anthropic API.
    [[nodiscard]] static QMap<QString, QString> authHeaders(const QString &apiKey);

    /// Build the messages array (system extracted to top level).
    [[nodiscard]] static QJsonArray buildMessages(
        const QList<act::core::LLMMessage> &messages);

private:
    /// Convert a single message to Anthropic content blocks.
    [[nodiscard]] static QJsonObject messageToAnthropic(const act::core::LLMMessage &msg);
};

} // namespace act::services
