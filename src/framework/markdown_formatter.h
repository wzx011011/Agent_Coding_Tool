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

    /// Format a single line of Markdown for terminal display.
    /// Handles inline code, bold, headings, lists, and horizontal rules.
    /// Does NOT add a trailing newline — the caller controls line termination.
    [[nodiscard]] static QString formatLine(const QString &line,
                                           bool colorEnabled = false);

    /// Format fenced code blocks — public so StreamFormatter can reuse it.
    [[nodiscard]] static QString formatCodeBlocks(const QString &text,
                                                   bool colorEnabled);

private:
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
