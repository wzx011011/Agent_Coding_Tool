#pragma once

#include <QString>

namespace act::framework
{

/// Converts Markdown text to terminal-friendly formatted output.
/// Uses plain-text conventions that work across all terminals
/// (no ANSI escape codes for maximum compatibility).
class MarkdownFormatter
{
public:
    /// Format a Markdown string for terminal display.
    [[nodiscard]] static QString format(const QString &markdown);

private:
    /// Format fenced code blocks (```...```)
    [[nodiscard]] static QString formatCodeBlocks(const QString &text);

    /// Format inline code (`...`)
    [[nodiscard]] static QString formatInlineCode(const QString &text);

    /// Format headings (# ## ### etc.)
    [[nodiscard]] static QString formatHeadings(const QString &text);

    /// Format bold (**...**)
    [[nodiscard]] static QString formatBold(const QString &text);

    /// Format unordered lists (- or * items)
    [[nodiscard]] static QString formatLists(const QString &text);

    /// Add horizontal rule formatting
    [[nodiscard]] static QString formatHorizontalRules(const QString &text);
};

} // namespace act::framework
