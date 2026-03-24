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

// ---- Utility ----

/// Strip all ANSI escape sequences from text (useful in tests / pipe mode).
[[nodiscard]] QString stripAnsi(const QString &text);

} // namespace TerminalStyle
} // namespace act::framework
