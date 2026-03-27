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
const char *const FG_BRIGHT_GREEN = "\x1b[92m";

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

QString fgBrightGreen(const QString &text)
{
    return s_colorEnabled ? QString::fromUtf8(FG_BRIGHT_GREEN) + text + QString::fromUtf8(RESET)
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
    return fgBrightGreen(QStringLiteral("\xe2\x97\x8f ")) + name + args;
}

QString toolCallRunning(const QString &name, const QString &args)
{
    return fgGray(QStringLiteral("  ")) + dim(name + args) +
           dim(QStringLiteral(" ..."));
}

QString toolCallCompleted(const QString &name,
                           const QString &summary,
                           bool success)
{
    if (success)
        return fgBrightGreen(QStringLiteral("\xe2\x97\x8f ")) + dim(QStringLiteral("  ") + summary);
    return fgRed(QStringLiteral("\xe2\x97\x8f ")) + dim(QStringLiteral("  ") + summary);
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

// ---- Channel display helpers ----

QString channelUserMessage(const QString &channelName,
                           const QString &senderId,
                           const QString &text)
{
    return dim(QStringLiteral("[%1] ").arg(channelName)) +
           boldYellow(senderId) + QStringLiteral(": ") + text;
}

QString channelPrefix(const QString &channelName)
{
    return dim(QStringLiteral("[%1] ").arg(channelName));
}

// ---- Rich streaming helpers ----

QString thinkingIndicator(const QString &spinnerChar)
{
    return fgBrightGreen(QStringLiteral("\xe2\x97\x8f Thinking")) + dim(QStringLiteral(" ") + spinnerChar);
}

QString sectionIndicator(bool collapsed)
{
    return dim(collapsed ? QStringLiteral("\xe2\x96\xb6") : QStringLiteral("\xe2\x96\xbc"));
}

QString resultBox(const QString &title, const QStringList &lines)
{
    static constexpr int MAX_LINES = 20;
    const auto displayLines = lines.size() > MAX_LINES
                                  ? lines.mid(0, MAX_LINES)
                                  : lines;

    int contentWidth = 0;
    for (const auto &line : displayLines)
        if (line.size() > contentWidth)
            contentWidth = static_cast<int>(line.size());
    int boxWidth = (title.size() + 4 > contentWidth + 4) ? title.size() + 4 : contentWidth + 4;

    // Box-drawing characters (Unicode codepoints for UTF-16)
    const QChar HORZ(0x2500);  // ─
    const QChar VERT(0x2502);  // │
    const QChar TLC(0x250C);   // ┌
    const QChar TRC(0x2510);   // ┐
    const QChar BLC(0x2514);   // └
    const QChar BRC(0x2518);   // ┘

    QString raw;
    // Top border: ┌─ title ─────────┐
    raw += TLC;
    raw += QString(2, HORZ) + QLatin1Char(' ') + title + QLatin1Char(' ');
    int remaining = boxWidth - title.size() - 4;
    if (remaining > 0)
        raw += QString(remaining, HORZ);
    raw += TRC;
    raw += QLatin1Char('\n');

    // Content lines: │  content          │
    for (const auto &line : displayLines)
    {
        raw += VERT;
        raw += QStringLiteral("  ") + line;
        int pad = boxWidth - static_cast<int>(line.size()) - 4;
        if (pad > 0)
            raw += QString(pad, QLatin1Char(' '));
        raw += QStringLiteral("  ") + VERT;
        raw += QLatin1Char('\n');
    }

    // Truncation notice
    if (lines.size() > MAX_LINES)
    {
        QString notice = QStringLiteral("  ... (%1 more lines)").arg(lines.size() - MAX_LINES);
        raw += VERT + notice;
        int pad = boxWidth - static_cast<int>(notice.size()) - 2;
        if (pad > 0)
            raw += QString(pad, QLatin1Char(' '));
        raw += VERT;
        raw += QLatin1Char('\n');
    }

    // Bottom border: └──────────────────┘
    raw += BLC;
    raw += QString(boxWidth, HORZ);
    raw += BRC;

    return dim(raw);
}

// ---- Diff formatting ----

QString diffAddedLine(const QString &line)
{
    return fgGreen(QStringLiteral("+") + line);
}

QString diffRemovedLine(const QString &line)
{
    return fgRed(QStringLiteral("-") + line);
}

QString diffContextLine(const QString &line)
{
    return dim(QStringLiteral(" ") + line);
}

QString formatDiff(const QString &diffOutput)
{
    const QStringList lines = diffOutput.split(QLatin1Char('\n'));
    QStringList formatted;
    for (const auto &line : lines)
    {
        if (line.startsWith(QLatin1Char('+')) && !line.startsWith(QStringLiteral("+++")))
            formatted.append(diffAddedLine(line.mid(1)));
        else if (line.startsWith(QLatin1Char('-')) && !line.startsWith(QStringLiteral("---")))
            formatted.append(diffRemovedLine(line.mid(1)));
        else if (line.startsWith(QLatin1Char(' ')))
            formatted.append(diffContextLine(line.mid(1)));
        else if (line.startsWith(QStringLiteral("+++")) ||
                 line.startsWith(QStringLiteral("---")) ||
                 line.startsWith(QStringLiteral("@@")))
            formatted.append(dim(line));
        else
            formatted.append(line);
    }
    return formatted.join(QLatin1Char('\n'));
}

// ---- Utility ----

QString stripAnsi(const QString &text)
{
    static const QRegularExpression ansiRe(
        QStringLiteral("\x1b\\[[0-9;]*m"));
    return QString(text).remove(ansiRe);
}

QString clearLine()
{
    return QString::fromUtf8("\r\x1b[2K");
}

} // namespace act::framework::TerminalStyle
