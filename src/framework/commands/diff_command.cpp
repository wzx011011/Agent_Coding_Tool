#include "framework/commands/diff_command.h"

#include "framework/command_registry.h"
#include "framework/terminal_style.h"
#include "infrastructure/interfaces.h"

namespace act::framework::commands {

void DiffCommand::registerTo(CommandRegistry &registry,
                              infrastructure::IProcess &process,
                              OutputCallback output)
{
    (void)registry.registerCommand(
        QStringLiteral("diff"),
        QStringLiteral("Show git diff with colored output (/diff [--staged|--stat|<file>])"),
        [&process, output](const QStringList &args) -> bool {
            QStringList gitArgs = {QStringLiteral("diff")};

            if (!args.isEmpty())
            {
                const auto &first = args.at(0);
                if (first == QLatin1String("--staged"))
                    gitArgs.append(QStringLiteral("--staged"));
                else if (first == QLatin1String("--stat"))
                    gitArgs.append(QStringLiteral("--stat"));
                else
                    gitArgs.append(first); // file path
            }

            process.execute(
                QStringLiteral("git"),
                gitArgs,
                [output](int exitCode, QString stdoutOutput) {
                    if (exitCode != 0)
                    {
                        output(TerminalStyle::errorMessage(
                            QStringLiteral("DIFF_ERROR"),
                            stdoutOutput.trimmed()));
                        return;
                    }

                    if (stdoutOutput.trimmed().isEmpty())
                    {
                        output(TerminalStyle::systemMessage(
                            QStringLiteral("No changes detected.")));
                        return;
                    }

                    // Apply ANSI colors via TerminalStyle::formatDiff
                    output(TerminalStyle::formatDiff(stdoutOutput));
                });

            return true;
        });
}

} // namespace act::framework::commands
