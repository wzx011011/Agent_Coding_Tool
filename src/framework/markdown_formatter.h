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
    /// When colorEnabled is false (default), output matches the original plain text.
    [[nodiscard]] static QString format(const QString &markdown,
                                         bool colorEnabled = false);

private:
    /// Format fenced code blocks (```...```)
    [[nodiscard]] static QString formatCodeBlocks(const QString &text,
                                                   bool colorEnabled);

    /// Format inline code (`...`)
    [[nodiscard]] static QString formatInlineCode(const QString &text,
                                                   bool colorEnabled);

    /// Format headings (# ## ### etc.)
    [[nodiscard]] static QString formatHeadings(const QString &text,
                                                bool colorEnabled);

    /// Format bold (**...**)
    [[nodiscard]] static QString formatBold(const QString &text,
                                             bool colorEnabled);

    /// Format unordered lists (- or * items)
    [[nodiscard]] static QString formatLists(const QString &text);

    /// Add horizontal rule formatting
    [[nodiscard]] static QString formatHorizontalRules(const QString &text,
                                                       bool colorEnabled);
};

} // namespace act::framework
