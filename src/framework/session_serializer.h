#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "core/types.h"

namespace act::framework
{

struct SessionMetadata
{
    QString model;
    QString provider;
    int totalTokens = 0;
    int inputTokens = 0;
    int outputTokens = 0;
    qint64 durationMs = 0;
    QDateTime exportedAt;
    QString exportFormat = QStringLiteral("1.0");
};

class SessionSerializer
{
public:
    /// Serialize messages to JSON.
    [[nodiscard]] static QJsonObject toJson(
        const QList<act::core::LLMMessage> &messages,
        const SessionMetadata &metadata = {});

    /// Deserialize JSON to messages.
    [[nodiscard]] static QList<act::core::LLMMessage> fromJson(
        const QJsonObject &json);

    /// Save session to a JSON file. Returns true on success.
    static bool saveToFile(const QString &path,
                           const QList<act::core::LLMMessage> &messages,
                           const SessionMetadata &metadata = {});

    /// Load session from a JSON file. Returns empty object on failure.
    [[nodiscard]] static QJsonObject loadFromFile(const QString &path);

    /// Export messages as human-readable Markdown.
    [[nodiscard]] static QString toMarkdown(
        const QList<act::core::LLMMessage> &messages,
        const SessionMetadata &metadata = {});

    /// Validate format version compatibility.
    [[nodiscard]] static bool validateFormat(const QJsonObject &json,
                                             QString *error = nullptr);

private:
    [[nodiscard]] static QJsonObject messageToJson(
        const act::core::LLMMessage &msg);
    [[nodiscard]] static act::core::LLMMessage jsonToMessage(
        const QJsonObject &obj);

    static constexpr auto kFormatIdentifier =
        QLatin1String("act-session-v1");
};

} // namespace act::framework
