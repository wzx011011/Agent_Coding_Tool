#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <optional>
#include <QString>
#include <vector>

namespace act::infrastructure::feishu
{

/// A parsed Feishu WebSocket event.
struct FeishuEvent
{
    QString eventId;
    QString eventType;
    QString token;    // for ACK response
    QJsonObject payload;
};

/// A user message extracted from an im.message.receive_v1 event.
struct FeishuMessage
{
    QString messageId;
    QString chatId;
    QString chatType;   // "p2p" or "group"
    QString senderId;
    QString content;    // decoded plain text
};

/// Parse a raw WebSocket JSON frame into a typed FeishuEvent.
/// Returns nullopt if the frame is not a valid event envelope.
[[nodiscard]] std::optional<FeishuEvent> parseEvent(const QJsonObject &frame);

/// Extract a FeishuMessage from an im.message.receive_v1 event.
/// Returns nullopt if the event type doesn't match or fields are missing.
[[nodiscard]] std::optional<FeishuMessage> extractMessage(const FeishuEvent &event);

/// Build the ACK JSON that must be sent back within 3 seconds.
[[nodiscard]] QJsonObject buildAckResponse(const QString &eventToken, const QString &eventId);

/// Result of sending a message via Feishu REST API.
struct FeishuSendResponse
{
    bool success = false;
    int statusCode = 0;
    QString code;       // Feishu error code ("0" on success)
    QString msg;        // Error message
    QString messageId;  // ID of the sent message
};

// ============================================================
// Protobuf frame decoder for Feishu WebSocket binary protocol
// ============================================================
//
// The Feishu WS uses protobuf-encoded frames (pbbp2.Frame):
//   field 1 (varint):  SeqID
//   field 2 (varint):  LogID
//   field 3 (zigzag):  service
//   field 4 (zigzag):  method (0=Control, 1=Data)
//   field 5 (message): Header { key(1)=string, value(2)=string }
//   field 6 (string):  payload_encoding
//   field 7 (string):  payload_type
//   field 8 (bytes):   payload (JSON for events)
//   field 9 (string):  LogIDNew

/// Decoded protobuf frame from Feishu WebSocket.
struct PbFrame
{
    int32_t method = -1;               // 0=Control, 1=Data
    int32_t service = 0;
    QMap<QString, QString> headers;    // header key-value pairs
    QByteArray payload;                // raw payload bytes
};

/// Minimal protobuf decoder for Feishu WS frames.
/// No external protobuf dependency needed — handles only the subset
/// of wire types required by pbbp2.Frame.
namespace pb
{

/// Decode a protobuf varint at offset, return (value, new_offset).
[[nodiscard]] std::pair<uint64_t, size_t> decodeVarint(const QByteArray &data, size_t offset);

/// Decode a zigzag-encoded signed int32.
[[nodiscard]] int32_t decodeZigzag(uint64_t value);

/// Decode a length-delimited field, return (bytes, new_offset).
[[nodiscard]] std::pair<QByteArray, size_t> decodeLengthDelimited(const QByteArray &data, size_t offset);

/// Decode a single Header message { key(1)=string, value(2)=string }.
[[nodiscard]] std::pair<QString, QString> decodeHeader(const QByteArray &data);

/// Decode a full Frame from raw binary data.
[[nodiscard]] PbFrame decodeFrame(const QByteArray &data);

/// Encode a ping Frame (method=0, type=ping) as protobuf bytes.
[[nodiscard]] QByteArray encodePingFrame(int32_t serviceId);

} // namespace pb

} // namespace act::infrastructure::feishu
