#include "framework/commands/copy_command.h"

#include <QDir>
#include <QProcess>

#include "core/types.h"
#include "framework/command_registry.h"
#include "framework/terminal_style.h"
#include "infrastructure/interfaces.h"

namespace act::framework::commands {

namespace {

/// Copy text to system clipboard using platform-specific commands via QProcess.
/// Returns true on success.
bool copyToClipboard(const QString &text)
{
    QProcess proc;

#if defined(Q_OS_WIN)
    proc.setProgram(QStringLiteral("cmd.exe"));
    proc.setArguments({QStringLiteral("/c"), QStringLiteral("clip")});
#elif defined(Q_OS_MACOS)
    proc.setProgram(QStringLiteral("pbcopy"));
#elif defined(Q_OS_LINUX)
    proc.setProgram(QStringLiteral("xclip"));
    proc.setArguments({QStringLiteral("-selection"), QStringLiteral("clipboard")});
#else
    return false;
#endif

    proc.start();
    if (!proc.waitForStarted(3000))
        return false;

    proc.write(text.toUtf8());
    proc.closeWriteChannel();
    if (!proc.waitForFinished(5000))
    {
        proc.kill();
        return false;
    }

    return proc.exitCode() == 0;
}

/// Strip basic markdown formatting from text.
QString stripMarkdown(const QString &text)
{
    QString result = text;

    // Remove code block fences
    result.remove(QStringLiteral("```"));

    // Remove bold/italic markers
    result.remove(QStringLiteral("***"));
    result.remove(QStringLiteral("**"));
    result.remove(QStringLiteral("*"));

    // Remove inline code backticks
    result.remove(QStringLiteral("`"));

    // Remove heading markers at line start
    QStringList lines = result.split(QStringLiteral("\n"));
    for (auto &line : lines)
    {
        while (line.startsWith(QStringLiteral("#")))
            line.remove(0, 1);
        if (line.startsWith(QStringLiteral(" ")))
            line = line.mid(1);
    }

    return lines.join(QStringLiteral("\n"));
}

} // anonymous namespace

void CopyCommand::registerTo(CommandRegistry &registry,
                              infrastructure::IProcess &process,
                              OutputCallback output,
                              HistoryGetter historyGetter)
{
    (void)process; // clipboard uses QProcess directly for stdin piping

    (void)registry.registerCommand(
        QStringLiteral("copy"),
        QStringLiteral("Copy last LLM response to clipboard (/copy [--raw])"),
        [output, historyGetter](const QStringList &args) -> bool {
            const auto &messages = historyGetter();

            // Find the last assistant message
            QString lastContent;
            for (int i = messages.size() - 1; i >= 0; --i)
            {
                if (messages.at(i).role == act::core::MessageRole::Assistant)
                {
                    lastContent = messages.at(i).content;
                    break;
                }
            }

            if (lastContent.isEmpty())
            {
                output(TerminalStyle::systemMessage(
                    QStringLiteral("No assistant response to copy.")));
                return true;
            }

            // Check --raw flag
            bool raw = args.contains(QStringLiteral("--raw"));
            QString text = raw ? lastContent : stripMarkdown(lastContent);

            bool ok = copyToClipboard(text);
            if (ok)
            {
                output(TerminalStyle::fgGreen(
                    QStringLiteral("Copied %1 characters to clipboard.")
                        .arg(text.length())));
            }
            else
            {
                output(TerminalStyle::errorMessage(
                    QStringLiteral("CLIPBOARD_ERROR"),
                    QStringLiteral("Failed to copy to clipboard.")));
            }

            return true;
        });
}

} // namespace act::framework::commands
