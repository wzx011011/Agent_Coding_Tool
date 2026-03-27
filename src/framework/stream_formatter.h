#pragma once

#include <QObject>

#include <QString>

namespace act::framework
{

/// Buffers streaming tokens into lines and applies Markdown formatting
/// before emitting them for terminal display.
class StreamFormatter : public QObject
{
    Q_OBJECT
public:
    explicit StreamFormatter(QObject *parent = nullptr);

    /// Feed a raw streaming token into the formatter.
    /// Completed lines are emitted via formattedLineReady.
    void feedToken(const QString &token);

    /// Flush any remaining buffered content (incomplete line or open code block).
    void flush();

signals:
    /// Emitted when one or more formatted lines are ready for output.
    /// Each emitted string ends with \n.
    void formattedLineReady(const QString &formattedLines);

private:
    void processCompleteLine(const QString &line);
    void formatAndEmitCodeBlock();

    QString m_lineBuffer;
    bool m_inCodeBlock = false;
    QString m_codeBlockLang;
    QStringList m_codeBlockLines;
};

} // namespace act::framework
