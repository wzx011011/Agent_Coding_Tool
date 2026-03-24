#include "framework/terminal_style.h"

#include <QRegularExpression>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <cstdio>
#endif

namespace act::framework::TerminalStyle
{

namespace
{

bool s_colorEnabled = false;

const char *const RESET = "\x1b[0m";
const char *const BOLD = "\x1b[1m";
const char *const DIM = "\x1b[2m";
const char *const FG_BLACK = "\x1b[30m";
const char *const FG_RED = "\x1b[31m";
const char *const FG_GREEN = "\x1b[32m";
const char *const FG_YELLOW = "\x1b[33m";
const char *const FG_BLUE = "\x1b[34m";
const char *const FG_MAGENTA = "\x1b[35m";
const char *const FG_CYAN = "\x1b[36m";
const char *const FG_WHITE = "\x1b[37m";
const char *const FG_GRAY = "\x1b[90m";

} // anonymous namespace

void initialize(bool forceColor)
{
#ifdef _WIN32
    if (_isatty(_fileno(stdout)))
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode))
        {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        s_colorEnabled = true;
    }
#else
    if (isatty(STDOUT_FILENO))
    {
        s_colorEnabled = true;
    }
#endif

    if (forceColor)
        s_colorEnabled = true;
}

bool colorEnabled()
{
    return s_colorEnabled;
}

void setColorEnabled(bool enabled)
{
    s_colorEnabled = enabled;
}

// ---- ANSI builders ----

QString bold(const QString &text)
{
    return s_colorEnabled ? QString::fromUtf8(BOLD) + text + QString::fromUtf8(RESET)
                          : text;
}

QString dim(const QString &text)
{
    return s_colorEnabled ? QString::fromUtf8(DIM) + text + QString::fromUtf8(RESET)
                          : text;
}

QString fgCyan(const QString &text)
{
    return s_colorEnabled ? QString::fromUtf8(FG_CYAN) + text + QString::fromUtf8(RESET)
                          : text;
}

QString fgYellow(const QString &text)
{
    return s_colorEnabled ? QString::fromUtf8(FG_YELLOW) + text + QString::fromUtf8(RESET)
                          : text;
}

QString fgRed(const QString &text)
{
    return s_colorEnabled ? QString::fromUtf8(FG_RED) + text + QString::fromUtf8(RESET)
                          : text;
}

QString fgGreen(const QString &text)
{
    return s_colorEnabled ? QString::fromUtf8(FG_GREEN) + text + QString::fromUtf8(RESET)
                          : text;
}

QString fgMagenta(const QString &text)
{
    return s_colorEnabled ? QString::fromUtf8(FG_MAGENTA) + text + QString::fromUtf8(RESET)
                          : text;
}

QString fgGray(const QString &text)
{
    return s_colorEnabled ? QString::fromUtf8(FG_GRAY) + text + QString::fromUtf8(RESET)
                          : text;
}

QString boldCyan(const QString &text)
{
    return s_colorEnabled
        ? QString::fromUtf8("\x1b[1m\x1b[36m") + text + QString::fromUtf8(RESET)
        : text;
}

QString boldYellow(const QString &text)
{
    return s_colorEnabled
        ? QString::fromUtf8("\x1b[1m\x1b[33m") + text + QString::fromUtf8(RESET)
        : text;
}

QString boldRed(const QString &text)
{
    return s_colorEnabled
        ? QString::fromUtf8("\x1b[1m\x1b[31m") + text + QString::fromUtf8(RESET)
        : text;
}

QString boldGreen(const QString &text)
{
    return s_colorEnabled
        ? QString::fromUtf8("\x1b[1m\x1b[32m") + text + QString::fromUtf8(RESET)
        : text;
}

QString boldMagenta(const QString &text)
{
    return s_colorEnabled
        ? QString::fromUtf8("\x1b[1m\x1b[35m") + text + QString::fromUtf8(RESET)
        : text;
}

QString reset()
{
    return s_colorEnabled ? QString::fromUtf8(RESET) : QString();
}

// ---- Semantic helpers ----

QString userPrompt(const QString &input)
{
    return boldCyan(QStringLiteral("> ")) + input;
}

QString systemMessage(const QString &msg)
{
    return dim(QStringLiteral("[System] ")) + msg;
}

QString toolCallStarted(const QString &name, const QString &args)
{
    return boldYellow(QStringLiteral("> ")) + name + args;
}

QString toolCallCompleted(const QString &name,
                           const QString &summary,
                           bool success)
{
    if (success)
        return boldGreen(QStringLiteral("+ ")) + name + QStringLiteral(" ") + summary;
    return boldRed(QStringLiteral("x ")) + name + QStringLiteral(" ") + summary;
}

QString errorMessage(const QString &code, const QString &msg)
{
    return boldRed(QStringLiteral("[Error] ")) + msg;
}

QString permissionRequest(const QString &tool, const QString &level)
{
    return boldMagenta(QStringLiteral("? Allow ")) + tool +
           QStringLiteral(" [") + level + QStringLiteral("]?");
}

// ---- Utility ----

QString stripAnsi(const QString &text)
{
    static const QRegularExpression ansiRe(
        QStringLiteral("\x1b\\[[0-9;]*m"));
    return QString(text).remove(ansiRe);
}

} // namespace act::framework::TerminalStyle
