#pragma once

#include "core/types.h"
#include "infrastructure/sse_parser.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace act::services {

class ResponsesConverter {
  public:
    struct ParsedResponse {
        QString text;
        QList<act::core::ToolCall> toolCalls;
        QString status;
        QString errorCode;
        QString errorMessage;
    };

    [[nodiscard]] static QJsonObject toRequest(const QList<act::core::LLMMessage> &messages, const QString &model,
                                               const QList<QJsonObject> &toolDefs = {}, int maxOutputTokens = 4096);

    [[nodiscard]] static bool parseSseEvent(const act::infrastructure::SseEvent &event, ParsedResponse &response);

    [[nodiscard]] static ParsedResponse parseResponseBody(const QByteArray &responseBody);

    [[nodiscard]] static ParsedResponse finalize(const ParsedResponse &accumulated);

    [[nodiscard]] static QMap<QString, QString> authHeaders(const QString &apiKey);

  private:
    [[nodiscard]] static QJsonArray buildInput(const QList<act::core::LLMMessage> &messages, QString &instructions);
    [[nodiscard]] static QJsonArray contentParts(QStringView text, QStringView role);
    [[nodiscard]] static QJsonObject toolToDefinition(const QString &name, const QString &description,
                                                      const QJsonObject &schema);
    static void mergeParsedResponse(ParsedResponse &target, const ParsedResponse &update);
    static void appendOutputItem(const QJsonObject &item, ParsedResponse &response, bool allowMessageText);
};

} // namespace act::services