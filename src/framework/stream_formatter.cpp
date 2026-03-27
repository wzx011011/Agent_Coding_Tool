#include "framework/stream_formatter.h"

#include <QRegularExpression>

#include "framework/markdown_formatter.h"
#include "framework/terminal_style.h"

namespace act::framework
{

StreamFormatter::StreamFormatter(QObject *parent)
    : QObject(parent)
{
}

void StreamFormatter::feedToken(const QString &token)
{
    if (token.isEmpty())
        return;

    int pos = 0;
    while (pos < token.length())
    {
        int nlPos = token.indexOf(QLatin1Char('\n'), pos);
        if (nlPos == -1)
        {
            m_lineBuffer += token.mid(pos);
            break;
        }

        m_lineBuffer += token.mid(pos, nlPos - pos);
        processCompleteLine(m_lineBuffer);
        m_lineBuffer.clear();
        pos = nlPos + 1;
    }
}

void StreamFormatter::processCompleteLine(const QString &line)
{
    QString trimmed = line.trimmed();

    if (m_inCodeBlock)
    {
        if (trimmed == QLatin1String("```"))
        {
            m_inCodeBlock = false;
            formatAndEmitCodeBlock();
        }
        else
        {
            m_codeBlockLines.append(line);
        }
        return;
    }

    // Check for opening code fence
    static const QRegularExpression fenceRe(
        QStringLiteral("^```(\\w*)\\s*$"));
    auto fenceMatch = fenceRe.match(trimmed);
    if (fenceMatch.hasMatch())
    {
        m_inCodeBlock = true;
        m_codeBlockLang = fenceMatch.captured(1);
        m_codeBlockLines.clear();
        return;
    }

    bool colorEnabled = TerminalStyle::colorEnabled();
    QString formatted = MarkdownFormatter::formatLine(line, colorEnabled);
    emit formattedLineReady(formatted + QLatin1Char('\n'));
}

void StreamFormatter::formatAndEmitCodeBlock()
{
    bool colorEnabled = TerminalStyle::colorEnabled();

    QString synthetic = QStringLiteral("```") + m_codeBlockLang +
                        QStringLiteral("\n") +
                        m_codeBlockLines.join(QLatin1Char('\n')) +
                        QStringLiteral("\n```\n");

    QString formatted = MarkdownFormatter::formatCodeBlocks(
        synthetic, colorEnabled);

    // The formatCodeBlocks output starts with a blank line, remove it
    if (formatted.startsWith(QLatin1Char('\n')))
        formatted.remove(0, 1);

    emit formattedLineReady(formatted);

    m_codeBlockLang.clear();
    m_codeBlockLines.clear();
}

void StreamFormatter::flush()
{
    if (m_inCodeBlock && !m_codeBlockLines.isEmpty())
    {
        formatAndEmitCodeBlock();
    }

    if (!m_lineBuffer.isEmpty())
    {
        emit formattedLineReady(m_lineBuffer + QLatin1Char('\n'));
        m_lineBuffer.clear();
    }
}

} // namespace act::framework
