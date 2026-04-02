#include "framework/commands/doctor_command.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QStorageInfo>

#include "framework/command_registry.h"
#include "framework/terminal_style.h"
#include "infrastructure/interfaces.h"

#include <spdlog/spdlog.h>

namespace act::framework::commands {

namespace {

/// Status labels for each diagnostic check.
constexpr auto kOk   = "[OK]  ";
constexpr auto kWarn = "[WARN] ";
constexpr auto kFail = "[FAIL] ";

/// Run a command via IProcess and return its stdout synchronously.
/// Returns empty string on failure or timeout.
QString runSync(infrastructure::IProcess &process,
                const QString &cmd,
                const QStringList &args)
{
    QString result;
    bool done = false;

    process.execute(cmd, args,
        [&](int exitCode, QString output) {
            if (exitCode == 0)
                result = output.trimmed();
            done = true;
        });

    return result;
}

/// Parse "X.Y.Z" from a version string and compare major.minor >= required.
bool versionAtLeast(const QString &version, int reqMajor, int reqMinor)
{
    // Extract leading version pattern "major.minor"
    auto parts = version.split(QStringLiteral("."));
    if (parts.size() < 2)
        return false;

    bool ok1 = false, ok2 = false;
    int major = parts[0].toInt(&ok1);
    int minor = parts[1].toInt(&ok2);
    if (!ok1 || !ok2)
        return false;

    if (major > reqMajor)
        return true;
    if (major == reqMajor)
        return minor >= reqMinor;
    return false;
}

/// Format a single diagnostic line.
QString diagLine(const char *status, const QString &name, const QString &detail)
{
    auto s = QString::fromUtf8(status);
    if (status == kOk)
        s = TerminalStyle::fgGreen(s);
    else if (status == kWarn)
        s = TerminalStyle::fgYellow(s);
    else
        s = TerminalStyle::fgRed(s);

    return QStringLiteral("%1 %2 — %3").arg(s, name, detail);
}

} // anonymous namespace

void DoctorCommand::registerTo(CommandRegistry &registry,
                                infrastructure::IProcess &process,
                                OutputCallback output)
{
    (void)registry.registerCommand(
        QStringLiteral("doctor"),
        QStringLiteral("Run environment diagnostics"),
        [&process, output](const QStringList & /*args*/) -> bool {
            QStringList lines;
            lines.append(TerminalStyle::boldCyan(
                QStringLiteral("ACT Environment Diagnostics")));
            lines.append(QString());

            // 1. CMake
            {
                auto ver = runSync(process,
                    QStringLiteral("cmake"),
                    {QStringLiteral("--version")});
                if (ver.isEmpty())
                    lines.append(diagLine(kFail, QStringLiteral("CMake"),
                                          QStringLiteral("not found")));
                else
                {
                    // First line: "cmake version X.Y.Z"
                    auto firstLine = ver.split(QStringLiteral("\n"))[0];
                    auto ok = versionAtLeast(firstLine, 3, 28);
                    lines.append(diagLine(
                        ok ? kOk : kWarn,
                        QStringLiteral("CMake"),
                        firstLine + (ok ? QString() : QStringLiteral(" (need >= 3.28)"))));
                }
            }

            // 2. Ninja
            {
                auto ver = runSync(process,
                    QStringLiteral("ninja"),
                    {QStringLiteral("--version")});
                if (ver.isEmpty())
                    lines.append(diagLine(kFail, QStringLiteral("Ninja"),
                                          QStringLiteral("not found")));
                else
                {
                    bool ok = versionAtLeast(ver, 1, 11);
                    lines.append(diagLine(
                        ok ? kOk : kWarn,
                        QStringLiteral("Ninja"),
                        ver + (ok ? QString() : QStringLiteral(" (need >= 1.11)"))));
                }
            }

            // 3. Qt
            {
                auto qtDir = qEnvironmentVariable("QTDIR");
                QString qmakePath;
                if (!qtDir.isEmpty())
                    qmakePath = QDir(qtDir).filePath(QStringLiteral("bin/qmake.exe"));
                else
                    qmakePath = QStringLiteral("E:/Qt6.10/bin/qmake.exe");

                if (QFile::exists(qmakePath))
                    lines.append(diagLine(kOk, QStringLiteral("Qt"),
                                          qmakePath));
                else
                    lines.append(diagLine(kWarn, QStringLiteral("Qt"),
                                          QStringLiteral("qmake not found at %1").arg(qmakePath)));
            }

            // 4. MSVC
            {
                auto whereOut = runSync(process,
                    QStringLiteral("where"),
                    {QStringLiteral("cl")});
                if (whereOut.isEmpty())
                    lines.append(diagLine(kWarn, QStringLiteral("MSVC"),
                                          QStringLiteral("cl.exe not in PATH (run vcvarsall.bat first)")));
                else
                    lines.append(diagLine(kOk, QStringLiteral("MSVC"),
                                          QStringLiteral("cl.exe accessible")));
            }

            // 5. vcpkg
            {
                auto vcpkgRoot = qEnvironmentVariable("VCPKG_ROOT");
                if (vcpkgRoot.isEmpty())
                    lines.append(diagLine(kWarn, QStringLiteral("vcpkg"),
                                          QStringLiteral("VCPKG_ROOT not set")));
                else
                    lines.append(diagLine(kOk, QStringLiteral("vcpkg"),
                                          vcpkgRoot));
            }

            // 6. Git
            {
                auto ver = runSync(process,
                    QStringLiteral("git"),
                    {QStringLiteral("--version")});
                if (ver.isEmpty())
                    lines.append(diagLine(kFail, QStringLiteral("Git"),
                                          QStringLiteral("not found")));
                else
                    lines.append(diagLine(kOk, QStringLiteral("Git"), ver));
            }

            // 7. API keys (check existence only, never print values)
            {
                bool hasAnthropic = !qEnvironmentVariable("ACT_ANTHROPIC_API_KEY").isEmpty();
                bool hasZhipu = !qEnvironmentVariable("ACT_ZHIPU_API_KEY").isEmpty();
                if (hasAnthropic || hasZhipu)
                {
                    QStringList providers;
                    if (hasAnthropic)
                        providers.append(QStringLiteral("Anthropic"));
                    if (hasZhipu)
                        providers.append(QStringLiteral("Zhipu"));
                    lines.append(diagLine(kOk, QStringLiteral("API keys"),
                                          providers.join(QStringLiteral(", "))));
                }
                else
                {
                    lines.append(diagLine(kWarn, QStringLiteral("API keys"),
                                          QStringLiteral("no ACT_ANTHROPIC_API_KEY or ACT_ZHIPU_API_KEY set")));
                }
            }

            // 8. Disk space on current drive
            {
                QStorageInfo storage(QDir::currentPath());
                auto gb = storage.bytesAvailable() / (1024.0 * 1024.0 * 1024.0);
                if (gb < 1.0)
                    lines.append(diagLine(kWarn, QStringLiteral("Disk space"),
                                          QStringLiteral("%1 GB free (need >= 1 GB)")
                                              .arg(gb, 0, 'f', 1)));
                else
                    lines.append(diagLine(kOk, QStringLiteral("Disk space"),
                                          QStringLiteral("%1 GB free").arg(gb, 0, 'f', 1)));
            }

            lines.append(QString());

            for (const auto &line : lines)
                output(line);

            return true;
        });
}

} // namespace act::framework::commands
