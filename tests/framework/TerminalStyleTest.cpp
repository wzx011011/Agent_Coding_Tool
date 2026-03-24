#include <gtest/gtest.h>

#include "framework/terminal_style.h"

using namespace act::framework::TerminalStyle;

// Helper: build an ANSI escape QString without QStringLiteral issues on MSVC
static QString esc(int n)
{
    std::string seq = "\x1b[" + std::to_string(n) + "m";
    return QString::fromUtf8(seq.c_str());
}

// ============================================================
// Color state tests
// ============================================================

TEST(TerminalStyleTest, DefaultColorIsDisabled)
{
    EXPECT_FALSE(colorEnabled());
}

TEST(TerminalStyleTest, SetColorEnabled)
{
    setColorEnabled(true);
    EXPECT_TRUE(colorEnabled());
    setColorEnabled(false);
    EXPECT_FALSE(colorEnabled());
}

// ============================================================
// Plain-text (color off) — helpers return unstyled text
// ============================================================

TEST(TerminalStyleTest, BoldOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(bold(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, DimOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(dim(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, FgCyanOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(fgCyan(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, FgYellowOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(fgYellow(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, FgRedOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(fgRed(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, FgGreenOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(fgGreen(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, FgMagentaOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(fgMagenta(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, FgGrayOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(fgGray(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, BoldCyanOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(boldCyan(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, BoldYellowOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(boldYellow(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, BoldRedOffReturnsPlain)
{
    setColorEnabled(false);
    EXPECT_EQ(boldRed(QStringLiteral("hello")), QStringLiteral("hello"));
}

TEST(TerminalStyleTest, ResetOffReturnsEmpty)
{
    setColorEnabled(false);
    EXPECT_EQ(reset(), QString());
}

// ============================================================
// ANSI generation (color on)
// ============================================================

TEST(TerminalStyleTest, BoldOnGeneratesAnsi)
{
    setColorEnabled(true);
    QString result = bold(QStringLiteral("hello"));
    EXPECT_TRUE(result.contains(esc(0)));
    EXPECT_TRUE(result.contains(QStringLiteral("hello")));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, DimOnGeneratesAnsi)
{
    setColorEnabled(true);
    QString result = dim(QStringLiteral("world"));
    EXPECT_TRUE(result.contains(esc(0)));
    EXPECT_TRUE(result.contains(QStringLiteral("world")));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, FgCyanOnGeneratesAnsi)
{
    setColorEnabled(true);
    QString result = fgCyan(QStringLiteral("test"));
    EXPECT_TRUE(result.contains(esc(36)));
    EXPECT_TRUE(result.contains(QStringLiteral("test")));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, FgGreenOnGeneratesAnsi)
{
    setColorEnabled(true);
    QString result = fgGreen(QStringLiteral("ok"));
    EXPECT_TRUE(result.contains(esc(32)));
    EXPECT_TRUE(result.contains(QStringLiteral("ok")));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, FgRedOnGeneratesAnsi)
{
    setColorEnabled(true);
    QString result = fgRed(QStringLiteral("err"));
    EXPECT_TRUE(result.contains(esc(31)));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, FgYellowOnGeneratesAnsi)
{
    setColorEnabled(true);
    QString result = fgYellow(QStringLiteral("warn"));
    EXPECT_TRUE(result.contains(esc(33)));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, FgMagentaOnGeneratesAnsi)
{
    setColorEnabled(true);
    QString result = fgMagenta(QStringLiteral("perm"));
    EXPECT_TRUE(result.contains(esc(35)));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, FgGrayOnGeneratesAnsi)
{
    setColorEnabled(true);
    QString result = fgGray(QStringLiteral("dim"));
    EXPECT_TRUE(result.contains(esc(90)));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, BoldCyanOnGeneratesAnsi)
{
    setColorEnabled(true);
    QString result = boldCyan(QStringLiteral("hi"));
    EXPECT_TRUE(result.contains(esc(1)));
    EXPECT_TRUE(result.contains(esc(36)));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, ResetOnReturnsResetSequence)
{
    setColorEnabled(true);
    QString result = reset();
    EXPECT_EQ(result, esc(0));
    setColorEnabled(false);
}

// ============================================================
// Semantic helpers — color off
// ============================================================

TEST(TerminalStyleTest, UserPromptOffFormat)
{
    setColorEnabled(false);
    QString result = userPrompt(QStringLiteral("hello world"));
    EXPECT_EQ(result, QStringLiteral("> hello world"));
}

TEST(TerminalStyleTest, SystemMessageOffFormat)
{
    setColorEnabled(false);
    QString result = systemMessage(QStringLiteral("Conversation reset."));
    EXPECT_EQ(result, QStringLiteral("[System] Conversation reset."));
}

TEST(TerminalStyleTest, ToolCallStartedOffFormat)
{
    setColorEnabled(false);
    QString result = toolCallStarted(
        QStringLiteral("file_read"), QStringLiteral(" /src/main.cpp"));
    EXPECT_EQ(result, QStringLiteral("> file_read /src/main.cpp"));
}

TEST(TerminalStyleTest, ToolCallCompletedSuccessOffFormat)
{
    setColorEnabled(false);
    QString result = toolCallCompleted(
        QStringLiteral("file_read"), QStringLiteral("(42 lines)"), true);
    EXPECT_EQ(result, QStringLiteral("+ file_read (42 lines)"));
}

TEST(TerminalStyleTest, ToolCallCompletedFailOffFormat)
{
    setColorEnabled(false);
    QString result = toolCallCompleted(
        QStringLiteral("shell"), QStringLiteral("[PERMISSION_DENIED]"), false);
    EXPECT_EQ(result, QStringLiteral("x shell [PERMISSION_DENIED]"));
}

TEST(TerminalStyleTest, ErrorMessageOffFormat)
{
    setColorEnabled(false);
    QString result = errorMessage(
        QStringLiteral("TIMEOUT"), QStringLiteral("Request timed out"));
    EXPECT_EQ(result, QStringLiteral("[Error] Request timed out"));
}

TEST(TerminalStyleTest, PermissionRequestOffFormat)
{
    setColorEnabled(false);
    QString result = permissionRequest(
        QStringLiteral("shell_exec"), QStringLiteral("Exec"));
    EXPECT_EQ(result, QStringLiteral("? Allow shell_exec [Exec]?"));
}

// ============================================================
// Semantic helpers — color on
// ============================================================

TEST(TerminalStyleTest, UserPromptOnContainsAnsi)
{
    setColorEnabled(true);
    QString result = userPrompt(QStringLiteral("test"));
    EXPECT_TRUE(result.contains(esc(0)));
    EXPECT_TRUE(result.contains(QStringLiteral("test")));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, ToolCallCompletedSuccessOnContainsGreen)
{
    setColorEnabled(true);
    QString result = toolCallCompleted(
        QStringLiteral("tool"), QStringLiteral("(1 lines)"), true);
    EXPECT_TRUE(result.contains(esc(32))); // green
    setColorEnabled(false);
}

TEST(TerminalStyleTest, ToolCallCompletedFailOnContainsRed)
{
    setColorEnabled(true);
    QString result = toolCallCompleted(
        QStringLiteral("tool"), QStringLiteral("[ERR]"), false);
    EXPECT_TRUE(result.contains(esc(31))); // red
    setColorEnabled(false);
}

// ============================================================
// stripAnsi
// ============================================================

TEST(TerminalStyleTest, StripAnsiRemovesEscapeSequences)
{
    setColorEnabled(true);
    QString colored = bold(QStringLiteral("hello")) + QStringLiteral(" world");
    QString stripped = stripAnsi(colored);
    EXPECT_EQ(stripped, QStringLiteral("hello world"));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, StripAnsiNoEscapePassthrough)
{
    QString plain = QStringLiteral("no escapes here");
    EXPECT_EQ(stripAnsi(plain), plain);
}

TEST(TerminalStyleTest, StripAnsiMultipleSequences)
{
    setColorEnabled(true);
    QString complex = userPrompt(QStringLiteral("test"));
    QString stripped = stripAnsi(complex);
    EXPECT_EQ(stripped, QStringLiteral("> test"));
    setColorEnabled(false);
}

TEST(TerminalStyleTest, StripAnsiEmptyString)
{
    EXPECT_EQ(stripAnsi(QString()), QString());
}
