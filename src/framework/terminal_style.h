#pragma once

#include <QString>

namespace act::framework
{

/// ANSI terminal style utilities.
/// Pure namespace (no QObject) for headless P1 CLI compatibility.
/// When color is disabled, all helpers return plain text matching
/// the original unstyled output, so existing tests remain valid.
namespace TerminalStyle
{

/// Initialize terminal support.
/// On Windows, enables ENABLE_VIRTUAL_TERMINAL_PROCESSING when stdout is a TTY.
/// Sets the global color-enabled flag based on TTY detection (unless forceColor is set).
void initialize(bool forceColor = false);

/// Query whether color output is active.
[[nodiscard]] bool colorEnabled();

/// Override the color flag (e.g. --no-color).
void setColorEnabled(bool enabled);

// ---- ANSI builders (return plain text when color is off) ----

[[nodiscard]] QString bold(const QString &text);
[[nodiscard]] QString dim(const QString &text);
[[nodiscard]] QString fgCyan(const QString &text);
[[nodiscard]] QString fgYellow(const QString &text);
[[nodiscard]] QString fgRed(const QString &text);
[[nodiscard]] QString fgGreen(const QString &text);
[[nodiscard]] QString fgMagenta(const QString &text);
[[nodiscard]] QString fgGray(const QString &text);
[[nodiscard]] QString boldCyan(const QString &text);
[[nodiscard]] QString boldYellow(const QString &text);
[[nodiscard]] QString boldRed(const QString &text);
[[nodiscard]] QString boldGreen(const QString &text);
[[nodiscard]] QString boldMagenta(const QString &text);
[[nodiscard]] QString fgBrightGreen(const QString &text);
[[nodiscard]] QString reset();

// ---- Semantic helpers ----

[[nodiscard]] QString userPrompt(const QString &input);
[[nodiscard]] QString systemMessage(const QString &msg);
[[nodiscard]] QString toolCallStarted(const QString &name, const QString &args);
[[nodiscard]] QString toolCallCompleted(const QString &name,
                                         const QString &summary,
                                         bool success);
[[nodiscard]] QString errorMessage(const QString &code, const QString &msg);
[[nodiscard]] QString permissionRequest(const QString &tool,
                                          const QString &level);

// ---- Channel display helpers ----

/// Format an incoming user message from a channel.
/// Example: "[Feishu] user_abc: Hello"
[[nodiscard]] QString channelUserMessage(const QString &channelName,
                                          const QString &senderId,
                                          const QString &text);

/// Channel prefix for AI response streaming.
/// Example: "[Feishu] " (styled)
[[nodiscard]] QString channelPrefix(const QString &channelName);

// ---- Rich streaming helpers ----

/// Thinking/waiting spinner indicator: e.g. "● Thinking ⠋"
[[nodiscard]] QString thinkingIndicator(const QString &spinnerChar = QStringLiteral("⠋"));

/// Collapsible section arrow: "▶" (collapsed) or "▼" (expanded)
[[nodiscard]] QString sectionIndicator(bool collapsed);

/// Bordered result box for tool output display.
[[nodiscard]] QString resultBox(const QString &title, const QStringList &lines);

// ---- Utility ----

/// Strip all ANSI escape sequences from text (useful in tests / pipe mode).
[[nodiscard]] QString stripAnsi(const QString &text);

/// CR + clear to end of line. Used by spinner to erase the thinking indicator.
[[nodiscard]] QString clearLine();

} // namespace TerminalStyle
} // namespace act::framework
