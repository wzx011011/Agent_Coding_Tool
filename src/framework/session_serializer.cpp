#include "framework/session_serializer.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <spdlog/spdlog.h>

namespace act::framework
{

namespace
{

QString roleToString(act::core::MessageRole role)
{
    switch (role)
    {
    case act::core::MessageRole::System:
        return QStringLiteral("system");
    case act::core::MessageRole::User:
        return QStringLiteral("user");
    case act::core::MessageRole::Assistant:
        return QStringLiteral("assistant");
    case act::core::MessageRole::Tool:
        return QStringLiteral("tool");
    }
    return QStringLiteral("unknown");
}

act::core::MessageRole stringToRole(const QString &str)
{
    if (str == QStringLiteral("system"))
        return act::core::MessageRole::System;
    if (str == QStringLiteral("user"))
        return act::core::MessageRole::User;
    if (str == QStringLiteral("assistant"))
        return act::core::MessageRole::Assistant;
    if (str == QStringLiteral("tool"))
        return act::core::MessageRole::Tool;
    return act::core::MessageRole::User;
}

} // anonymous namespace

QJsonObject SessionSerializer::messageToJson(const act::core::LLMMessage &msg)
{
    QJsonObject obj;
    obj[QStringLiteral("role")] = roleToString(msg.role);
    obj[QStringLiteral("content")] = msg.content;

    // Tool call ID (role == Tool)
    if (msg.role == act::core::MessageRole::Tool && !msg.toolCallId.isEmpty())
    {
        obj[QStringLiteral("toolCallId")] = msg.toolCallId;
    }

    // Tool calls (role == Assistant)
    if (msg.role == act::core::MessageRole::Assistant)
    {
        const auto &calls = msg.toolCalls;
        if (!calls.isEmpty())
        {
            QJsonArray callsArray;
            for (const auto &tc : calls)
            {
                QJsonObject callObj;
                callObj[QStringLiteral("id")] = tc.id;
                callObj[QStringLiteral("name")] = tc.name;
                callObj[QStringLiteral("params")] = tc.params;
                callsArray.append(callObj);
            }
            obj[QStringLiteral("toolCalls")] = callsArray;
        }

        // Backward compat: single toolCall stored at top level
        if (calls.size() == 1)
        {
            const auto &tc = calls.first();
            QJsonObject singleCall;
            singleCall[QStringLiteral("id")] = tc.id;
            singleCall[QStringLiteral("name")] = tc.name;
            singleCall[QStringLiteral("params")] = tc.params;
            obj[QStringLiteral("toolCall")] = singleCall;
        }
    }

    return obj;
}

act::core::LLMMessage SessionSerializer::jsonToMessage(const QJsonObject &obj)
{
    act::core::LLMMessage msg;
    msg.role = stringToRole(obj[QStringLiteral("role")].toString());
    msg.content = obj[QStringLiteral("content")].toString();

    // Tool call ID
    msg.toolCallId = obj[QStringLiteral("toolCallId")].toString();

    // Tool calls array
    const QJsonArray callsArray =
        obj[QStringLiteral("toolCalls")].toArray();
    for (const auto &val : callsArray)
    {
        const QJsonObject callObj = val.toObject();
        act::core::ToolCall tc;
        tc.id = callObj[QStringLiteral("id")].toString();
        tc.name = callObj[QStringLiteral("name")].toString();
        tc.params = callObj[QStringLiteral("params")].toObject();
        msg.toolCalls.append(tc);
    }

    // Backward compat: single toolCall
    if (msg.toolCalls.isEmpty() &&
        obj.contains(QStringLiteral("toolCall")))
    {
        const QJsonObject singleCall =
            obj[QStringLiteral("toolCall")].toObject();
        act::core::ToolCall tc;
        tc.id = singleCall[QStringLiteral("id")].toString();
        tc.name = singleCall[QStringLiteral("name")].toString();
        tc.params = singleCall[QStringLiteral("params")].toObject();
        msg.toolCall = tc;
        msg.toolCalls.append(tc);
    }

    return msg;
}

QJsonObject SessionSerializer::toJson(
    const QList<act::core::LLMMessage> &messages,
    const SessionMetadata &metadata)
{
    QJsonObject root;
    root[QStringLiteral("format")] = kFormatIdentifier;

    QJsonObject metaObj;
    metaObj[QStringLiteral("model")] = metadata.model;
    metaObj[QStringLiteral("provider")] = metadata.provider;
    metaObj[QStringLiteral("totalTokens")] = metadata.totalTokens;
    metaObj[QStringLiteral("inputTokens")] = metadata.inputTokens;
    metaObj[QStringLiteral("outputTokens")] = metadata.outputTokens;
    metaObj[QStringLiteral("durationMs")] = metadata.durationMs;
    metaObj[QStringLiteral("exportFormat")] = metadata.exportFormat;

    if (metadata.exportedAt.isValid())
    {
        metaObj[QStringLiteral("exportedAt")] =
            metadata.exportedAt.toString(Qt::ISODate);
    }
    else
    {
        metaObj[QStringLiteral("exportedAt")] =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }

    root[QStringLiteral("metadata")] = metaObj;

    QJsonArray messagesArray;
    for (const auto &msg : messages)
    {
        messagesArray.append(messageToJson(msg));
    }
    root[QStringLiteral("messages")] = messagesArray;

    return root;
}

QList<act::core::LLMMessage> SessionSerializer::fromJson(
    const QJsonObject &json)
{
    QList<act::core::LLMMessage> result;

    const QJsonArray messagesArray =
        json[QStringLiteral("messages")].toArray();
    result.reserve(messagesArray.size());

    for (const auto &val : messagesArray)
    {
        result.append(jsonToMessage(val.toObject()));
    }

    return result;
}

bool SessionSerializer::saveToFile(const QString &path,
                                   const QList<act::core::LLMMessage> &messages,
                                   const SessionMetadata &metadata)
{
    const QJsonObject json = toJson(messages, metadata);
    const QJsonDocument doc(json);
    const QByteArray data = doc.toJson(QJsonDocument::Indented);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        spdlog::error("SessionSerializer: failed to open file for writing: {}",
                      path.toStdString());
        return false;
    }

    const qint64 written = file.write(data);
    if (written != data.size())
    {
        spdlog::error(
            "SessionSerializer: incomplete write to {}: {}/{} bytes",
            path.toStdString(), written, data.size());
        return false;
    }

    file.close();
    spdlog::info("SessionSerializer: saved {} messages to {}",
                 messages.size(), path.toStdString());
    return true;
}

QJsonObject SessionSerializer::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        spdlog::error("SessionSerializer: failed to open file for reading: {}",
                      path.toStdString());
        return {};
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        spdlog::error("SessionSerializer: JSON parse error at offset {}: {}",
                      parseError.offset,
                      parseError.errorString().toStdString());
        return {};
    }

    if (!doc.isObject())
    {
        spdlog::error("SessionSerializer: JSON root is not an object");
        return {};
    }

    return doc.object();
}

QString SessionSerializer::toMarkdown(
    const QList<act::core::LLMMessage> &messages,
    const SessionMetadata &metadata)
{
    QString md;
    md += QStringLiteral("# ACT Session Export\n\n");

    // Metadata section
    md += QStringLiteral("## Metadata\n\n");
    md += QStringLiteral("- **Model**: %1\n").arg(metadata.model);
    md += QStringLiteral("- **Provider**: %1\n").arg(metadata.provider);
    md += QStringLiteral("- **Tokens**: %1 in / %2 out / %3 total\n")
              .arg(metadata.inputTokens)
              .arg(metadata.outputTokens)
              .arg(metadata.totalTokens);
    md += QStringLiteral("- **Duration**: %1 ms\n").arg(metadata.durationMs);

    const QDateTime exportTime = metadata.exportedAt.isValid()
                                     ? metadata.exportedAt
                                     : QDateTime::currentDateTimeUtc();
    md += QStringLiteral("- **Exported**: %1\n")
              .arg(exportTime.toString(Qt::ISODate));
    md += QStringLiteral("\n---\n\n");

    // Messages
    md += QStringLiteral("## Conversation\n\n");
    for (const auto &msg : messages)
    {
        QString roleName;
        switch (msg.role)
        {
        case act::core::MessageRole::System:
            roleName = QStringLiteral("System");
            break;
        case act::core::MessageRole::User:
            roleName = QStringLiteral("User");
            break;
        case act::core::MessageRole::Assistant:
            roleName = QStringLiteral("Assistant");
            break;
        case act::core::MessageRole::Tool:
            roleName = QStringLiteral("Tool");
            break;
        }

        md += QStringLiteral("### %1\n\n").arg(roleName);

        if (!msg.content.isEmpty())
        {
            md += msg.content;
            md += QStringLiteral("\n\n");
        }

        // Tool calls
        if (msg.role == act::core::MessageRole::Assistant &&
            !msg.toolCalls.isEmpty())
        {
            md += QStringLiteral("**Tool Calls:**\n\n");
            for (const auto &tc : msg.toolCalls)
            {
                md += QStringLiteral("- `%1` (id: `%2`)\n")
                          .arg(tc.name, tc.id);
                if (!tc.params.isEmpty())
                {
                    md += QStringLiteral("  - Params: `%1`\n")
                              .arg(QString::fromUtf8(
                                  QJsonDocument(tc.params).toJson(
                                      QJsonDocument::Compact)));
                }
            }
            md += QStringLiteral("\n");
        }

        // Tool result
        if (msg.role == act::core::MessageRole::Tool &&
            !msg.toolCallId.isEmpty())
        {
            md += QStringLiteral("*(tool call id: `%1`)*\n\n")
                      .arg(msg.toolCallId);
        }
    }

    return md;
}

bool SessionSerializer::validateFormat(const QJsonObject &json,
                                       QString *error)
{
    const QString format =
        json[QStringLiteral("format")].toString();

    if (format != kFormatIdentifier)
    {
        if (error)
        {
            *error = QStringLiteral("Unsupported format '%1'. Expected '%2'.")
                         .arg(format, QString(kFormatIdentifier));
        }
        return false;
    }

    if (!json.contains(QStringLiteral("messages")))
    {
        if (error)
        {
            *error = QStringLiteral("Missing 'messages' field.");
        }
        return false;
    }

    if (!json[QStringLiteral("messages")].isArray())
    {
        if (error)
        {
            *error = QStringLiteral("'messages' field must be an array.");
        }
        return false;
    }

    return true;
}

} // namespace act::framework
