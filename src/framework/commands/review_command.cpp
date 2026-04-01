#include "framework/commands/review_command.h"

#include <QRegularExpression>
#include <QStringBuilder>

#include <spdlog/spdlog.h>

#include "framework/command_registry.h"
#include "framework/terminal_style.h"
#include "infrastructure/interfaces.h"

namespace act::framework::commands {

namespace {

/// Synchronous helper: run a process command and capture output.
bool runSync(infrastructure::IProcess &process,
             const QString &cmd,
             const QStringList &args,
             QString &outOutput,
             int timeoutMs = 15000)
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
    return success && done;
}

// ---- Static analysis check definitions ----

struct ReviewCheck
{
    QString name;
    QString severity;     // "C" = Critical, "W" = Warning, "I" = Info
    QRegularExpression pattern;
    QString description;
    QString fixSuggestion;
};

/// Get all static analysis checks for C++/Qt code review.
QList<ReviewCheck> getChecks()
{
    return {
        // Type safety
        {
            QStringLiteral("C-style cast"),
            QStringLiteral("W"),
            QRegularExpression(QStringLiteral(R"(\(\w+\s*\*?\s*\)\s*\w+)")),
            QStringLiteral("C-style cast detected"),
            QStringLiteral("Use static_cast<>/reinterpret_cast<> instead")
        },
        {
            QStringLiteral("raw new/delete"),
            QStringLiteral("W"),
            QRegularExpression(QStringLiteral(R"(\bnew\s+\w+)")),
            QStringLiteral("Raw new/delete detected"),
            QStringLiteral("Use std::make_unique/std::make_shared or Qt parent-child")
        },
        {
            QStringLiteral("raw delete"),
            QStringLiteral("W"),
            QRegularExpression(QStringLiteral(R"(\bdelete\s+\w+)")),
            QStringLiteral("Raw delete detected"),
            QStringLiteral("Use smart pointers or Qt parent-child ownership")
        },

        // Naming conventions
        {
            QStringLiteral("qDebug in production"),
            QStringLiteral("W"),
            QRegularExpression(QStringLiteral(R"(\bqDebug\s*\(\))")),
            QStringLiteral("qDebug() used in production code"),
            QStringLiteral("Use spdlog for logging")
        },
        {
            QStringLiteral("magic number"),
            QStringLiteral("I"),
            QRegularExpression(QStringLiteral(R"(\b\d{2,}\b)")),
            QStringLiteral("Possible magic number"),
            QStringLiteral("Extract to named constant or constexpr")
        },
        {
            QStringLiteral("commented-out code"),
            QStringLiteral("I"),
            QRegularExpression(QStringLiteral(R"(^\+\s*//\s*(if|for|while|return|auto|void|int|bool|QString))")),
            QStringLiteral("Commented-out code detected"),
            QStringLiteral("Remove dead code or use #if 0 block")
        },

        // Qt-specific
        {
            QStringLiteral("missing QStringLiteral"),
            QStringLiteral("I"),
            QRegularExpression(QStringLiteral(
                R"(QJsonObject\s*\[\s*"[^"]+"\s*\])")),
            QStringLiteral("QJsonObject access without QStringLiteral"),
            QStringLiteral("Use QStringLiteral(\"key\") for QJsonObject::operator[]")
        },
        {
            QStringLiteral("unhandled QJsonObject access"),
            QStringLiteral("W"),
            QRegularExpression(QStringLiteral(
                R"(\w+\[QStringLiteral\("[^"]+"\)\]\.(toString|toInt|toBool|toDouble|toObject|toArray|toStringList)\(\))")),
            QStringLiteral("QJsonObject access without contains() check"),
            QStringLiteral("Call .contains() before accessing the key")
        },

        // Performance
        {
            QStringLiteral("pass by value"),
            QStringLiteral("I"),
            QRegularExpression(QStringLiteral(
                R"(\bvoid\s+\w+\([^&]*QString\s+\w+[^&]*\))")),
            QStringLiteral("QString passed by value"),
            QStringLiteral("Use const QString & for input parameters")
        },

        // Security
        {
            QStringLiteral("hardcoded credential pattern"),
            QStringLiteral("C"),
            QRegularExpression(QStringLiteral(
                R"((password|secret|api_key|apikey|token)\s*=\s*"[^"]{4,}")"),
                QRegularExpression::CaseInsensitiveOption),
            QStringLiteral("Possible hardcoded credential"),
            QStringLiteral("Use environment variables or config files")
        },
        {
            QStringLiteral("SQL injection pattern"),
            QStringLiteral("C"),
            QRegularExpression(QStringLiteral(
                R"(QStringLiteral\s*\(\s*"SELECT.*%1|INSERT.*%1|UPDATE.*%1|DELETE.*%1)"),
                QRegularExpression::CaseInsensitiveOption),
            QStringLiteral("Possible SQL injection via string formatting"),
            QStringLiteral("Use parameterized queries")
        },
        {
            QStringLiteral("command injection pattern"),
            QStringLiteral("C"),
            QRegularExpression(QStringLiteral(
                R"(system\s*\(\s*\w+)")),
            QStringLiteral("Possible command injection via system()"),
            QStringLiteral("Use QProcess with argument list instead")
        },

        // Style
        {
            QStringLiteral("using namespace std"),
            QStringLiteral("W"),
            QRegularExpression(QStringLiteral(R"(using\s+namespace\s+std\s*;)")),
            QStringLiteral("using namespace std in implementation"),
            QStringLiteral("Use full qualification (std::) or restrict to function scope")
        },
    };
}

/// Represents a single finding from the review.
struct ReviewFinding
{
    QString severity;   // "C", "W", "I"
    QString checkName;
    QString file;
    int line = 0;
    QString description;
    QString fixSuggestion;
};

/// Parse diff output into per-file hunks and run checks.
void analyzeDiff(const QString &diffOutput, QList<ReviewFinding> &findings)
{
    auto checks = getChecks();
    const auto lines = diffOutput.split(QLatin1Char('\n'));

    QString currentFile;
    int currentLine = 0;

    for (const auto &line : lines)
    {
        // Detect file header: "--- a/path/to/file.cpp" or "+++ b/path/to/file.cpp"
        static const QRegularExpression fileAddedRe(
            QStringLiteral(R"(^\+\+\+ b/(.+)$))"));
        auto fileMatch = fileAddedRe.match(line);
        if (fileMatch.hasMatch())
        {
            currentFile = fileMatch.captured(1);
            continue;
        }

        // Detect hunk header: "@@ -a,b +c,d @@"
        static const QRegularExpression hunkRe(
            QStringLiteral(R"(^@@@? -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@@?)"));
        auto hunkMatch = hunkRe.match(line);
        if (hunkMatch.hasMatch())
        {
            currentLine = hunkMatch.captured(1).toInt();
            continue;
        }

        // Track line numbers
        if (line.startsWith(QLatin1Char('+')) && !line.startsWith(QStringLiteral("+++")))
        {
            currentLine++;
            // Only check added lines (+ prefix)
            QString content = line.mid(1); // strip leading '+'

            for (const auto &check : checks)
            {
                if (content.contains(check.pattern))
                {
                    ReviewFinding f;
                    f.severity = check.severity;
                    f.checkName = check.name;
                    f.file = currentFile;
                    f.line = currentLine;
                    f.description = check.description;
                    f.fixSuggestion = check.fixSuggestion;
                    findings.append(std::move(f));
                }
            }
        }
        else if (line.startsWith(QLatin1Char(' ')))
        {
            currentLine++;
        }
    }
}

/// Count insertions and deletions from diff output.
void countDiffLines(const QString &diffOutput, int &ins, int &del)
{
    ins = 0;
    del = 0;
    const auto lines = diffOutput.split(QLatin1Char('\n'));
    for (const auto &line : lines)
    {
        if (line.startsWith(QLatin1Char('+')) && !line.startsWith(QStringLiteral("+++")))
            ++ins;
        else if (line.startsWith(QLatin1Char('-')) && !line.startsWith(QStringLiteral("---")))
            ++del;
    }
}

/// Count distinct files from diff output.
int countFiles(const QString &diffOutput)
{
    int count = 0;
    static const QRegularExpression fileRe(QString(R"(^\+\+\+ b/(.+)$)"));
    const auto lines = diffOutput.split(QLatin1Char('\n'));
    for (const auto &line : lines)
    {
        if (fileRe.match(line).hasMatch())
            ++count;
    }
    return count;
}

} // anonymous namespace

void ReviewCommand::registerTo(CommandRegistry &registry,
                               infrastructure::IProcess &process,
                               OutputCallback output)
{
    registry.registerCommand(
        QStringLiteral("review"),
        QStringLiteral("Code review with static analysis (options: --staged, <path>)"),
        [&process, output](const QStringList &args) -> bool {
            const bool staged = args.contains(QStringLiteral("--staged"));

            // Extract path argument (anything not a flag)
            QString path;
            for (const auto &a : args)
            {
                if (!a.startsWith(QLatin1Char('-')))
                {
                    path = a;
                    break;
                }
            }

            // Build git diff command
            QStringList diffArgs = {QStringLiteral("diff")};
            if (staged)
                diffArgs.append(QStringLiteral("--staged"));
            if (!path.isEmpty())
            {
                diffArgs.append(QStringLiteral("--"));
                diffArgs.append(path);
            }

            QString diffOutput;
            if (!runSync(process, QStringLiteral("git"), diffArgs, diffOutput))
            {
                output(TerminalStyle::errorMessage(
                    QStringLiteral("GIT_ERROR"),
                    QStringLiteral("[GIT_ERROR] Failed to run git diff. Are you in a git repository?")));
                return true;
            }

            if (diffOutput.trimmed().isEmpty())
            {
                output(TerminalStyle::systemMessage(
                    QStringLiteral("No changes to review.")));
                return true;
            }

            // Analyze
            QList<ReviewFinding> findings;
            analyzeDiff(diffOutput, findings);

            int fileCount = countFiles(diffOutput);
            int ins = 0, del = 0;
            countDiffLines(diffOutput, ins, del);

            // Count severities
            int critical = 0, warnings = 0, info = 0;
            for (const auto &f : findings)
            {
                if (f.severity == QStringLiteral("C"))
                    ++critical;
                else if (f.severity == QStringLiteral("W"))
                    ++warnings;
                else
                    ++info;
            }

            // Build report
            QString report;
            report += QStringLiteral("## Code Review: %1 file(s), +%2/-%3 lines\n\n")
                .arg(fileCount).arg(ins).arg(del);

            // Group findings by severity
            auto appendGroup = [&](const QString &severity, const QString &label) {
                bool hasAny = false;
                for (const auto &f : findings)
                {
                    if (f.severity != severity)
                        continue;
                    if (!hasAny)
                    {
                        report += QStringLiteral("### %1 (%2)\n").arg(label, severity);
                        hasAny = true;
                    }
                    report += QStringLiteral("- [ ] `%1:%2`: %3")
                        .arg(f.file)
                        .arg(f.line)
                        .arg(f.description);
                    if (!f.fixSuggestion.isEmpty())
                        report += QStringLiteral(" -- %1").arg(f.fixSuggestion);
                    report += QLatin1Char('\n');
                }
                if (hasAny)
                    report += QLatin1Char('\n');
            };

            appendGroup(QStringLiteral("C"), QStringLiteral("Critical"));
            appendGroup(QStringLiteral("W"), QStringLiteral("Warning"));
            appendGroup(QStringLiteral("I"), QStringLiteral("Info"));

            // Summary
            QString overall = (critical > 0) ? QStringLiteral("NEEDS ATTENTION")
                                             : QStringLiteral("PASS");

            report += QStringLiteral("### Summary\n");
            report += QStringLiteral("- Files reviewed: %1\n").arg(fileCount);
            report += QStringLiteral("- Issues found: %1 critical, %2 warnings, %3 info\n")
                .arg(critical).arg(warnings).arg(info);
            report += QStringLiteral("- Overall: %1\n").arg(overall);

            output(report);
            return true;
        });
}

} // namespace act::framework::commands
