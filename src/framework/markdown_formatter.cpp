#include "framework/markdown_formatter.h"

#include <QRegularExpression>

#include "framework/terminal_style.h"

namespace act::framework
{

QString MarkdownFormatter::format(const QString &markdown, bool colorEnabled)
{
    if (markdown.isEmpty())
        return {};

    QString result = markdown;

    // Order matters: code blocks first (they should not be modified)
    result = formatCodeBlocks(result, colorEnabled);
    result = formatInlineCode(result, colorEnabled);
    result = formatHeadings(result, colorEnabled);
    result = formatBold(result, colorEnabled);
    result = formatLists(result);
    result = formatHorizontalRules(result, colorEnabled);

    return result;
}

QString MarkdownFormatter::formatCodeBlocks(const QString &text,
                                             bool colorEnabled)
{
    // Replace fenced code blocks with indented, bordered output
    static const QRegularExpression re(
        QStringLiteral("```(\\w*)\\n([^`]*)```"),
        QRegularExpression::DotMatchesEverythingOption);

    QString result;
    int lastEnd = 0;

    auto it = re.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();

        // Add text before the code block
        result += text.mid(lastEnd, match.capturedStart() - lastEnd);

        QString lang = match.captured(1);
        QString code = match.captured(2).trimmed();

        // Format as a bordered code block
        QString border = QStringLiteral("─").repeated(40);
        result += QStringLiteral("\n  ┌") + border + QStringLiteral("┐\n");

        if (!lang.isEmpty())
        {
            QString langDisplay = colorEnabled
                ? TerminalStyle::fgCyan(lang)
                : lang;
            QString label = QStringLiteral("  │ ") + langDisplay + QStringLiteral(":");
            result += label.leftJustified(44) + QStringLiteral("│\n");
            result += QStringLiteral("  ├") + border + QStringLiteral("┤\n");
        }

        const auto lines = code.split('\n');
        for (const auto &line : lines)
        {
            QString padded = QStringLiteral("  │ ") + line;
            result += padded.leftJustified(44) + QStringLiteral("│\n");
        }

        result += QStringLiteral("  └") + border + QStringLiteral("┘\n");

        lastEnd = match.capturedEnd();
    }

    // Add remaining text
    if (lastEnd < text.length())
        result += text.mid(lastEnd);

    return result;
}

QString MarkdownFormatter::formatInlineCode(const QString &text,
                                                 bool colorEnabled)
{
    static const QRegularExpression re(
        QStringLiteral("`([^`]+)`"));

    QString result;
    int lastEnd = 0;

    auto it = re.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        result += text.mid(lastEnd, match.capturedStart() - lastEnd);

        QString code = match.captured(1);
        if (colorEnabled)
            code = TerminalStyle::fgCyan(code);
        result += QStringLiteral(" [") + code + QStringLiteral("] ");

        lastEnd = match.capturedEnd();
    }

    if (lastEnd < text.length())
        result += text.mid(lastEnd);

    return result;
}

QString MarkdownFormatter::formatHeadings(const QString &text,
                                              bool colorEnabled)
{
    QString result;
    const auto lines = text.split('\n');

    for (const auto &line : lines)
    {
        QString trimmed = line.trimmed();

        // Match ATX-style headings: # heading
        static const QRegularExpression headingRe(
            QStringLiteral("^(#{1,6})\\s+(.+)"));

        auto match = headingRe.match(trimmed);
        if (match.hasMatch())
        {
            int level = match.captured(1).length();
            QString title = match.captured(2);

            if (colorEnabled)
                title = TerminalStyle::bold(title);

            QString underline;
            if (level <= 2)
                underline = QStringLiteral("═").repeated(
                    qMin(title.length(), 60));
            else
                underline = QStringLiteral("─").repeated(
                    qMin(title.length(), 60));

            if (colorEnabled)
                underline = TerminalStyle::dim(underline);

            if (level == 1)
            {
                result += QStringLiteral("\n  ") + title + QStringLiteral("\n  ") + underline + QStringLiteral("\n\n");
            }
            else if (level == 2)
            {
                result += QStringLiteral("\n  ") + title + QStringLiteral("\n  ") + underline + QStringLiteral("\n\n");
            }
            else
            {
                result += QStringLiteral("  ") + title + QStringLiteral("\n  ") + underline + QStringLiteral("\n");
            }
        }
        else
        {
            result += line + QStringLiteral("\n");
        }
    }

    return result;
}

QString MarkdownFormatter::formatBold(const QString &text,
                                          bool colorEnabled)
{
    static const QRegularExpression re(
        QStringLiteral("\\*\\*([^*]+)\\*\\*"));

    QString result;
    int lastEnd = 0;

    auto it = re.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        result += text.mid(lastEnd, match.capturedStart() - lastEnd);

        QString content = match.captured(1);
        result += colorEnabled
            ? TerminalStyle::bold(content)
            : content.toUpper();

        lastEnd = match.capturedEnd();
    }

    if (lastEnd < text.length())
        result += text.mid(lastEnd);

    return result;
}

QString MarkdownFormatter::formatLists(const QString &text)
{
    QString result;
    const auto lines = text.split('\n');

    int bulletIndex = 0;

    for (const auto &line : lines)
    {
        QString trimmed = line.trimmed();

        // Unordered list items: - item or * item
        static const QRegularExpression listRe(
            QStringLiteral("^[-*]\\s+(.+)"));

        auto match = listRe.match(trimmed);
        if (match.hasMatch())
        {
            QString bullet = QStringLiteral("•");
            result += QStringLiteral("  ") + bullet + QStringLiteral(" ") +
                      match.captured(1) + QStringLiteral("\n");
            ++bulletIndex;
        }
        else
        {
            bulletIndex = 0;
            result += line + QStringLiteral("\n");
        }
    }

    return result;
}

QString MarkdownFormatter::formatHorizontalRules(const QString &text,
                                                       bool colorEnabled)
{
    static const QRegularExpression re(
        QStringLiteral("^\\s*[-*_]{3,}\\s*$"),
        QRegularExpression::MultilineOption);

    QString separator = QString::fromUtf8("─").repeated(40);
    if (colorEnabled)
        separator = TerminalStyle::dim(separator);

    QString result = text;
    QString replacement = QStringLiteral("\n  ") + separator + QStringLiteral("\n");
    result.replace(re, replacement);
    return result;
}

} // namespace act::framework
