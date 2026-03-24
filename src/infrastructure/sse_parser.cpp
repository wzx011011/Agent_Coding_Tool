#include "infrastructure/sse_parser.h"

namespace act::infrastructure
{

SseParser::SseParser() = default;

QList<SseEvent> SseParser::feed(const QByteArray &chunk)
{
    QList<SseEvent> events;
    m_buffer.append(chunk);

    auto lines = splitLines();
    for (auto &line : lines)
    {
        processLine(std::move(line), events);
    }

    return events;
}

QList<SseEvent> SseParser::flush()
{
    QList<SseEvent> events;

    // Process any remaining data in the buffer as a final line
    if (!m_buffer.isEmpty())
    {
        processLine(m_buffer, events);
        m_buffer.clear();
    }

    // If there's still incomplete event data, emit it
    if (!m_currentData.isEmpty())
    {
        SseEvent event;
        event.eventType = m_currentEventType;
        event.data = m_currentData;
        events.append(event);
        resetCurrent();
    }

    return events;
}

void SseParser::reset()
{
    m_buffer.clear();
    resetCurrent();
}

QList<QByteArray> SseParser::splitLines()
{
    QList<QByteArray> result;
    QByteArray line;

    for (int i = 0; i < m_buffer.size(); ++i)
    {
        char c = m_buffer[i];
        if (c == '\n')
        {
            // Remove trailing \r
            if (!line.isEmpty() && line.endsWith('\r'))
                line.chop(1);
            result.append(line);
            line.clear();
        }
        else if (c == '\r')
        {
            // \r without \n: treat as line break
            result.append(line);
            line.clear();
        }
        else
        {
            line.append(c);
        }
    }

    // Keep incomplete final line in buffer
    m_buffer = line;
    return result;
}

void SseParser::processLine(QByteArray line, QList<SseEvent> &events)
{
    // Empty line = event boundary
    if (line.isEmpty())
    {
        if (!m_currentData.isEmpty())
        {
            SseEvent event;
            event.eventType = m_currentEventType;
            event.data = m_currentData;
            events.append(event);
            resetCurrent();
        }
        return;
    }

    // Comment line (starts with ':')
    if (line.startsWith(':'))
        return;

    // Parse field
    int colonPos = line.indexOf(':');
    if (colonPos < 0)
    {
        // Field with no value (e.g., "event" alone)
        QString field = QString::fromUtf8(line).trimmed();
        if (field == QStringLiteral("event"))
        {
            // Reset event type
            m_currentEventType.clear();
        }
        return;
    }

    QString field = QString::fromUtf8(line.left(colonPos)).trimmed();
    QByteArray value = line.mid(colonPos + 1);
    // Remove leading space after colon (SSE spec)
    if (!value.isEmpty() && value.startsWith(' '))
        value = value.mid(1);

    if (field == QLatin1String("event"))
    {
        m_currentEventType = QString::fromUtf8(value);
    }
    else if (field == QLatin1String("data"))
    {
        if (m_currentData.isEmpty())
            m_currentData = QString::fromUtf8(value);
        else
            m_currentData += QLatin1Char('\n') + QString::fromUtf8(value);
    }
    else if (field == QLatin1String("id"))
    {
        m_lastEventId = QString::fromUtf8(value);
    }
    // Ignore "retry" field
}

void SseParser::resetCurrent()
{
    m_currentEventType.clear();
    m_currentData.clear();
}

} // namespace act::infrastructure
