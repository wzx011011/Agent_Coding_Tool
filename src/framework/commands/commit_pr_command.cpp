#include "framework/commands/commit_pr_command.h"

#include <QRegularExpression>
#include <QStringBuilder>

#include <spdlog/spdlog.h>

#include "framework/command_registry.h"
#include "framework/terminal_style.h"
#include "infrastructure/interfaces.h"

namespace act::framework::commands {

namespace {

/// Synchronous helper: run a process command and capture output.
/// Returns true on exit code 0.
bool runSync(infrastructure::IProcess &process,
             const QString &cmd,
             const QStringList &args,
             QString &outOutput,
             int timeoutMs = 10000)
{
    bool done = false;
    bool success = false;
    process.execute(cmd, args,
                    [&](int exitCode, QString output) {
                        outOutput = std::move(output);
                        success = (exitCode == 0);
                        done = true;
                    },
                    timeoutMs);
    // IProcess callback is synchronous in mock/test environments.
    // In production it fires via QEventLoop.
    return success && done;
}

/// Detect conventional commit type from file paths in the diff stat.
QString detectCommitType(const QStringList &changedFiles)
{
    bool hasSrc = false;
    bool hasTests = false;
    bool hasDocs = false;
    bool hasBuild = false;

    for (const auto &f : changedFiles)
    {
        if (f.startsWith(QStringLiteral("src/")))
            hasSrc = true;
        else if (f.startsWith(QStringLiteral("tests/")))
            hasTests = true;
        else if (f.endsWith(QStringLiteral(".md")))
            hasDocs = true;
        else if (f.contains(QStringLiteral("CMakeLists.txt")) ||
                 f.endsWith(QStringLiteral(".cmake")))
            hasBuild = true;
    }

    // Priority: test > docs > build > src
    if (hasTests && !hasSrc)
        return QStringLiteral("test");
    if (hasDocs && !hasSrc && !hasTests)
        return QStringLiteral("docs");
    if (hasBuild && !hasSrc && !hasTests)
        return QStringLiteral("chore");
    if (hasSrc)
        return QStringLiteral("feat");

    return QStringLiteral("chore");
}

/// Extract scope from file paths (e.g., "src/framework/commands/" -> "framework").
QString detectScope(const QStringList &changedFiles)
{
    if (changedFiles.isEmpty())
        return QString();

    // Take the first file, extract the second directory component after src/
    for (const auto &f : changedFiles)
    {
        if (f.startsWith(QStringLiteral("src/")))
        {
            auto parts = f.split(QLatin1Char('/'));
            if (parts.size() >= 3)
                return parts[1]; // e.g., "framework"
        }
    }
    return QString();
}

/// Parse file paths from `git diff --stat` output.
/// Lines look like: " src/foo.cpp | 12 +++---"
QStringList parseFilesFromStat(const QString &statOutput)
{
    QStringList files;
    const auto lines = statOutput.split(QLatin1Char('\n'));
    for (const auto &line : lines)
    {
        int pipeIdx = line.indexOf(QLatin1Char('|'));
        if (pipeIdx > 0)
        {
            QString file = line.left(pipeIdx).trimmed();
            if (!file.isEmpty())
                files.append(file);
        }
    }
    return files;
}

/// Parse insertions/deletions from `git diff --shortstat` output.
/// e.g., " 3 files changed, 42 insertions(+), 7 deletions(-)"
void parseShortstat(const QString &stat, int &files, int &ins, int &del)
{
    files = 0;
    ins = 0;
    del = 0;

    static const QRegularExpression reFiles(
        QStringLiteral("(\\d+) files? changed"));
    static const QRegularExpression reIns(
        QStringLiteral("(\\d+) insertion"));
    static const QRegularExpression reDel(
        QStringLiteral("(\\d+) deletion"));

    auto match = reFiles.match(stat);
    if (match.hasMatch())
        files = match.captured(1).toInt();

    match = reIns.match(stat);
    if (match.hasMatch())
        ins = match.captured(1).toInt();

    match = reDel.match(stat);
    if (match.hasMatch())
        del = match.captured(1).toInt();
}

} // anonymous namespace

void CommitPrCommand::registerTo(CommandRegistry &registry,
                                 infrastructure::IProcess &process,
                                 OutputCallback output)
{
    registry.registerCommand(
        QStringLiteral("commit-and-pr"),
        QStringLiteral("Commit staged changes and create a PR (options: --all, --draft)"),
        [&process, output](const QStringList &args) -> bool {
            const bool stageAll = args.contains(QStringLiteral("--all"));
            const bool isDraft = args.contains(QStringLiteral("--draft"));

            // Step 1: Check git status for changes
            QString statusOutput;
            if (!runSync(process, QStringLiteral("git"),
                         {QStringLiteral("status"), QStringLiteral("--porcelain")},
                         statusOutput))
            {
                output(TerminalStyle::errorMessage(
                    QStringLiteral("GIT_ERROR"),
                    QStringLiteral("Failed to run git status. Are you in a git repository?")));
                return true;
            }

            if (statusOutput.trimmed().isEmpty())
            {
                output(TerminalStyle::systemMessage(
                    QStringLiteral("No changes to commit.")));
                return true;
            }

            // Step 2: If --all, stage everything
            if (stageAll)
            {
                QString addOutput;
                if (!runSync(process, QStringLiteral("git"),
                             {QStringLiteral("add"), QStringLiteral("-A")},
                             addOutput))
                {
                    output(TerminalStyle::errorMessage(
                        QStringLiteral("GIT_ERROR"),
                        QStringLiteral("git add -A failed.")));
                    return true;
                }
            }

            // Step 3: Show staged diff stat
            QString statOutput;
            runSync(process, QStringLiteral("git"),
                    {QStringLiteral("diff"), QStringLiteral("--staged"), QStringLiteral("--stat")},
                    statOutput);

            if (statOutput.trimmed().isEmpty())
            {
                output(TerminalStyle::systemMessage(
                    QStringLiteral("No staged changes. Use /commit-and-pr --all to stage all.")));
                return true;
            }

            output(TerminalStyle::systemMessage(
                QStringLiteral("Staged changes:\n%1").arg(statOutput.trimmed())));

            // Step 4: Get shortstat for message generation
            QString shortstatOutput;
            runSync(process, QStringLiteral("git"),
                    {QStringLiteral("diff"), QStringLiteral("--staged"), QStringLiteral("--shortstat")},
                    shortstatOutput);

            int fileCount = 0, insertions = 0, deletions = 0;
            parseShortstat(shortstatOutput, fileCount, insertions, deletions);

            // Step 5: Generate commit message
            QStringList changedFiles = parseFilesFromStat(statOutput);
            QString type = detectCommitType(changedFiles);
            QString scope = detectScope(changedFiles);

            QString scopePart;
            if (!scope.isEmpty())
                scopePart = QStringLiteral("(%1)").arg(scope);

            QString summary;
            if (fileCount == 1)
                summary = QStringLiteral("update %1").arg(
                    changedFiles.isEmpty() ? QStringLiteral("1 file") : changedFiles.first());
            else
                summary = QStringLiteral("%1 files changed, +%2/-%3 lines")
                    .arg(fileCount)
                    .arg(insertions)
                    .arg(deletions);

            QString commitMsg = QStringLiteral("%1%2: %3")
                .arg(type, scopePart, summary);

            // Step 6: Commit
            QString commitOutput;
            if (!runSync(process, QStringLiteral("git"),
                         {QStringLiteral("commit"), QStringLiteral("-m"), commitMsg},
                         commitOutput, 15000))
            {
                output(TerminalStyle::errorMessage(
                    QStringLiteral("COMMIT_FAILED"),
                    QStringLiteral("[COMMIT_FAILED] git commit failed:\n%1").arg(commitOutput.trimmed())));
                return true;
            }

            output(TerminalStyle::boldGreen(
                QStringLiteral("Committed: %1").arg(commitMsg)));
            if (!commitOutput.trimmed().isEmpty())
            {
                output(commitOutput.trimmed());
            }

            // Step 7: Push to remote
            QString pushOutput;
            if (!runSync(process, QStringLiteral("git"),
                         {QStringLiteral("push"), QStringLiteral("-u"),
                          QStringLiteral("origin"), QStringLiteral("HEAD")},
                         pushOutput, 30000))
            {
                output(TerminalStyle::errorMessage(
                    QStringLiteral("PUSH_FAILED"),
                    QStringLiteral("[PUSH_FAILED] git push failed:\n%1").arg(pushOutput.trimmed())));
                return true;
            }

            output(TerminalStyle::fgGreen(
                QStringLiteral("Pushed to remote.")));
            if (!pushOutput.trimmed().isEmpty())
            {
                output(pushOutput.trimmed());
            }

            // Step 8: Check gh CLI availability
            QString ghVersionOutput;
            bool ghAvailable = runSync(process, QStringLiteral("gh"),
                                       {QStringLiteral("--version")},
                                       ghVersionOutput, 5000);

            if (!ghAvailable)
            {
                output(TerminalStyle::systemMessage(
                    QStringLiteral("Committed and pushed, but gh CLI not found. "
                                   "Create PR manually via GitHub.")));
                return true;
            }

            // Step 9: Create PR
            QStringList prArgs = {
                QStringLiteral("pr"),
                QStringLiteral("create"),
                QStringLiteral("--title"), commitMsg,
                QStringLiteral("--body"), QStringLiteral("Auto-generated PR from /commit-and-pr.\n\n%1")
                    .arg(shortstatOutput.trimmed())
            };
            if (isDraft)
                prArgs.append(QStringLiteral("--draft"));

            QString prOutput;
            if (!runSync(process, QStringLiteral("gh"), prArgs,
                         prOutput, 30000))
            {
                output(TerminalStyle::errorMessage(
                    QStringLiteral("PR_FAILED"),
                    QStringLiteral("[PR_FAILED] gh pr create failed:\n%1").arg(prOutput.trimmed())));
                return true;
            }

            // Parse PR URL from output
            static const QRegularExpression urlRe(
                QStringLiteral("https://github\\.com/\\S+/pull/\\d+"));
            auto urlMatch = urlRe.match(prOutput);
            if (urlMatch.hasMatch())
            {
                output(TerminalStyle::boldCyan(
                    QStringLiteral("Created PR: %1").arg(urlMatch.captured(0))));
            }
            else
            {
                output(TerminalStyle::boldCyan(
                    QStringLiteral("PR created:\n%1").arg(prOutput.trimmed())));
            }

            return true;
        });
}

} // namespace act::framework::commands
