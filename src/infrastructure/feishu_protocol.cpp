#include "infrastructure/feishu_protocol.h"

namespace act::infrastructure::feishu
{

std::optional<FeishuEvent> parseEvent(const QJsonObject &frame)
{
    // Feishu WS envelope: { "event_id", "event_type", "token", ... "event": {...} }
    // or the entire frame IS the event (depending on schema version).
    const QString eventId = frame.value(QStringLiteral("event_id")).toString();
    const QString eventType = frame.value(QStringLiteral("event_type")).toString();

    // Prefer header-based schema (v2):
    // { "header": { "event_id", "event_type", "token" }, "event": {...} }
    if (frame.contains(QStringLiteral("header")))
    {
        const auto header = frame.value(QStringLiteral("header")).toObject();
        FeishuEvent evt;
        evt.eventId = header.value(QStringLiteral("event_id")).toString();
        evt.eventType = header.value(QStringLiteral("event_type")).toString();
        evt.token = header.value(QStringLiteral("token")).toString();
        evt.payload = frame.value(QStringLiteral("event")).toObject();
        return evt;
    }

    // Flat schema (v1): top-level event_id, event_type
    if (!eventId.isEmpty() || !eventType.isEmpty())
    {
        FeishuEvent evt;
        evt.eventId = eventId;
        evt.eventType = eventType;
        evt.token = frame.value(QStringLiteral("token")).toString();
        evt.payload = frame;
        return evt;
    }

    return std::nullopt;
}

std::optional<FeishuMessage> extractMessage(const FeishuEvent &event)
{
    if (event.eventType != QStringLiteral("im.message.receive_v1"))
        return std::nullopt;

    const auto msgObj = event.payload.value(QStringLiteral("message")).toObject();
    if (msgObj.isEmpty())
        return std::nullopt;

    const auto senderObj = msgObj.value(QStringLiteral("sender")).toObject();
    const auto senderIdObj = senderObj.value(QStringLiteral("sender_id")).toObject();

    FeishuMessage msg;
    msg.messageId = msgObj.value(QStringLiteral("message_id")).toString();
    msg.chatId = msgObj.value(QStringLiteral("chat_id")).toString();
    msg.chatType = msgObj.value(QStringLiteral("chat_type")).toString();
    msg.senderId = senderIdObj.value(QStringLiteral("open_id")).toString();

    // content is a JSON string: "{\"text\":\"hello\"}"
    const QString rawContent = msgObj.value(QStringLiteral("content")).toString();
    if (!rawContent.isEmpty())
    {
        const auto contentDoc = QJsonDocument::fromJson(rawContent.toUtf8());
        if (contentDoc.isObject())
        {
            msg.content = contentDoc.object().value(QStringLiteral("text")).toString();
        }
        else
        {
            msg.content = rawContent;
        }
    }

    return msg;
}

QJsonObject buildAckResponse(const QString &eventToken, const QString &eventId)
{
    // For Feishu SDK mode WebSocket, the ACK is a JSON object with code 0.
    // Schema: { "code": 0, "event_id": "...", "token": "..." }
    QJsonObject ack;
    ack[QStringLiteral("code")] = 0;
    ack[QStringLiteral("event_id")] = eventId;
    if (!eventToken.isEmpty())
        ack[QStringLiteral("token")] = eventToken;
    return ack;
}

} // namespace act::infrastructure::feishu

namespace act::infrastructure::feishu::pb
{

std::pair<uint64_t, size_t> decodeVarint(const QByteArray &data, size_t offset)
{
    uint64_t result = 0;
    int shift = 0;
    while (offset < static_cast<size_t>(data.size()))
    {
        const auto byte = static_cast<uint8_t>(data[static_cast<int>(offset)]);
        result |= (static_cast<uint64_t>(byte & 0x7F)) << shift;
        ++offset;
        if ((byte & 0x80) == 0)
            break;
        shift += 7;
    }
    return {result, offset};
}

int32_t decodeZigzag(const uint64_t value)
{
    return static_cast<int32_t>((value >> 1) ^ (-(static_cast<int64_t>(value) & 1)));
}

std::pair<QByteArray, size_t> decodeLengthDelimited(const QByteArray &data, size_t offset)
{
    const auto [length, newOffset] = decodeVarint(data, offset);
    const auto len = static_cast<int>(length);
    if (newOffset + len > static_cast<size_t>(data.size()))
        return {{}, newOffset};
    return {data.mid(static_cast<int>(newOffset), len), newOffset + static_cast<size_t>(len)};
}

std::pair<QString, QString> decodeHeader(const QByteArray &data)
{
    QString key, value;
    size_t offset = 0;
    while (offset < static_cast<size_t>(data.size()))
    {
        const auto [tag, newOffset] = decodeVarint(data, offset);
        const int fieldNumber = static_cast<int>(tag >> 3);
        const int wireType = static_cast<int>(tag & 0x07);
        offset = newOffset;

        if (wireType == 2) // length-delimited
        {
            const auto [raw, nextOffset] = decodeLengthDelimited(data, offset);
            offset = nextOffset;
            if (fieldNumber == 1)
                key = QString::fromUtf8(raw);
            else if (fieldNumber == 2)
                value = QString::fromUtf8(raw);
        }
        else if (wireType == 0) // varint — skip
        {
            const auto [_, nextOffset] = decodeVarint(data, offset);
            offset = nextOffset;
        }
        else
        {
            break; // unknown wire type
        }
    }
    return {key, value};
}

PbFrame decodeFrame(const QByteArray &data)
{
    PbFrame frame;
    const auto size = static_cast<size_t>(data.size());
    size_t offset = 0;
    while (offset < size)
    {
        const auto [tag, newOffset] = decodeVarint(data, offset);
        if (tag == 0)
            break; // zero tag = end of stream
        const int fieldNumber = static_cast<int>(tag >> 3);
        const int wireType = static_cast<int>(tag & 0x07);
        offset = newOffset;

        if (wireType == 0) // varint
        {
            const auto [value, nextOffset] = decodeVarint(data, offset);
            offset = nextOffset;
            // Go SDK uses int32 (standard varint), NOT sint32 (zigzag)
            if (fieldNumber == 4)
                frame.method = static_cast<int32_t>(value);
            else if (fieldNumber == 3)
                frame.service = static_cast<int32_t>(value);
        }
        else if (wireType == 2) // length-delimited
        {
            const auto [raw, nextOffset] = decodeLengthDelimited(data, offset);
            offset = nextOffset;
            if (fieldNumber == 5) // Header
            {
                const auto [k, v] = decodeHeader(raw);
                frame.headers.insert(k, v);
            }
            else if (fieldNumber == 8) // payload
            {
                frame.payload = raw;
            }
        }
        else if (wireType == 1) // 64-bit (fixed64, sfixed64, double)
        {
            if (offset + 8 <= size)
                offset += 8;
            else
                break;
        }
        else if (wireType == 5) // 32-bit (fixed32, sfixed32, float)
        {
            if (offset + 4 <= size)
                offset += 4;
            else
                break;
        }
        else
        {
            break; // wire type 3/4 (groups) not expected
        }
    }
    return frame;
}

namespace
{
void encodeVarint(const uint64_t value, std::vector<uint8_t> &out)
{
    uint64_t v = value;
    do
    {
        uint8_t byte = static_cast<uint8_t>(v & 0x7F);
        v >>= 7;
        if (v != 0)
            byte |= 0x80;
        out.push_back(byte);
    } while (v != 0);
}

void encodeTag(const int fieldNumber, const int wireType, std::vector<uint8_t> &out)
{
    encodeVarint(static_cast<uint64_t>((fieldNumber << 3) | wireType), out);
}

void encodeLengthDelimited(const int fieldNumber, const QByteArray &value, std::vector<uint8_t> &out)
{
    encodeTag(fieldNumber, 2, out);
    encodeVarint(static_cast<uint64_t>(value.size()), out);
    for (int i = 0; i < value.size(); ++i)
        out.push_back(static_cast<uint8_t>(value[i]));
}

void encodeInt32(const int fieldNumber, int32_t value, std::vector<uint8_t> &out)
{
    encodeTag(fieldNumber, 0, out);
    encodeVarint(static_cast<uint64_t>(value), out);
}

QByteArray encodeHeader(const QString &key, const QString &value)
{
    std::vector<uint8_t> out;
    const QByteArray keyBytes = key.toUtf8();
    const QByteArray valBytes = value.toUtf8();
    encodeLengthDelimited(1, keyBytes, out);
    encodeLengthDelimited(2, valBytes, out);
    return QByteArray(reinterpret_cast<const char *>(out.data()), static_cast<int>(out.size()));
}

} // anonymous namespace

QByteArray encodePingFrame(const int32_t serviceId)
{
    std::vector<uint8_t> out;

    // SeqID = 0
    encodeTag(1, 0, out);
    encodeVarint(0, out);

    // LogID = 0
    encodeTag(2, 0, out);
    encodeVarint(0, out);

    // service
    encodeInt32(3, serviceId, out);

    // method = 0 (Control)
    encodeInt32(4, 0, out);

    // Header: type = "ping"
    const auto header = encodeHeader(QStringLiteral("type"), QStringLiteral("ping"));
    encodeLengthDelimited(5, header, out);

    return QByteArray(reinterpret_cast<const char *>(out.data()), static_cast<int>(out.size()));
}

} // namespace act::infrastructure::feishu::pb
