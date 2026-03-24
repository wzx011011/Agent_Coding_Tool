#pragma once

#include <QByteArray>
#include <QString>
#include <QList>

namespace act::infrastructure
{

/// Represents a single SSE event.
struct SseEvent
{
    QString eventType;  // e.g., "message_start", "content_block_delta" (Anthropic) or empty (standard)
    QString data;       // JSON payload
    QString lastEventId;
};

/// Incremental SSE protocol parser.
/// Supports both standard SSE (data: {...}\ndata: [DONE]) and
/// Anthropic SSE (event: type\ndata: {...}) formats.
class SseParser
{
public:
    SseParser();

    /// Feed a chunk of raw bytes from the HTTP stream.
    /// Returns a list of fully parsed events (may be empty if chunk is incomplete).
    [[nodiscard]] QList<SseEvent> feed(const QByteArray &chunk);

    /// Flush any incomplete event at end of stream.
    [[nodiscard]] QList<SseEvent> flush();

    /// Reset parser state completely.
    void reset();

    [[nodiscard]] const QString &lastEventId() const { return m_lastEventId; }

private:
    QList<QByteArray> splitLines();
    void processLine(QByteArray line, QList<SseEvent> &events);
    void resetCurrent();

    QByteArray m_buffer;
    QString m_currentEventType;
    QString m_currentData;
    QString m_lastEventId;
};

} // namespace act::infrastructure
