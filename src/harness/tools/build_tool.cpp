#include "harness/tools/build_tool.h"

#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

BuildTool::BuildTool(act::infrastructure::IProcess &proc,
                     QString workspaceRoot)
    : m_proc(proc)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString BuildTool::name() const
{
    return QStringLiteral("build");
}

QString BuildTool::description() const
{
    return QStringLiteral("Build the project using _build.bat. "
                          "Supports modes: full (default), build-only, "
                          "configure-only, no-test. "
                          "Returns structured errors on failure. "
                          "Requires exec permission.");
}

QJsonObject BuildTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("mode")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("enum")] = QJsonArray{
            QStringLiteral("full"),
            QStringLiteral("build-only"),
            QStringLiteral("configure-only"),
            QStringLiteral("no-test"),
        };
        obj[QStringLiteral("description")] =
            QStringLiteral("Build mode: 'full' (default), 'build-only', "
                          "'configure-only', or 'no-test'");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult BuildTool::execute(const QJsonObject &params)
{
    // Determine build mode
    QString buildArg;
    auto modeValue = params.value(QStringLiteral("mode"));
    if (modeValue.isString())
    {
        const QString mode = modeValue.toString();
        if (mode == QStringLiteral("build-only"))
            buildArg = QStringLiteral("--build");
        else if (mode == QStringLiteral("configure-only"))
            buildArg = QStringLiteral("--configure");
        else if (mode == QStringLiteral("no-test"))
            buildArg = QStringLiteral("--no-test");
        else if (mode != QStringLiteral("full"))
        {
            return act::core::ToolResult::err(
                act::core::errors::INVALID_PARAMS,
                QStringLiteral("Unknown build mode: '%1'. "
                               "Valid modes: full, build-only, "
                               "configure-only, no-test")
                    .arg(mode));
        }
    }

    const QString buildScript = QStringLiteral("_build.bat");

    QString output;
    int exitCode = -1;

    spdlog::info("BuildTool: running {} {}", buildScript.toStdString(),
                 buildArg.toStdString());

    // Execute _build.bat via cmd.exe /c
    // .bat files must be run through cmd.exe for proper execution
    QStringList args;
    args << QStringLiteral("/c") << buildScript;
    if (!buildArg.isEmpty())
        args << buildArg;

    m_proc.execute(QStringLiteral("cmd.exe"), args,
        [&output, &exitCode](int code, QString out) {
            exitCode = code;
            output = std::move(out);
        },
        m_timeoutMs);

    QJsonObject metadata;
    auto parseResult = parseBuildOutput(output);
    metadata = parseResult;

    if (exitCode == -1)
    {
        return act::core::ToolResult::err(
            act::core::errors::TIMEOUT,
            QStringLiteral("Build timed out after %1 ms")
                .arg(m_timeoutMs),
            metadata);
    }

    if (exitCode != 0)
    {
        // Truncate output for LLM consumption, but keep error lines
        QString summary = output;
        if (summary.length() > 3000)
        {
            // Keep first 500 and last 2000 chars
            summary = summary.left(500) +
                      QStringLiteral("\n... [truncated] ...\n") +
                      summary.right(2000);
        }
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("Build failed with exit code %1:\n%2")
                .arg(exitCode)
                .arg(summary),
            metadata);
    }

    // Success — return last 50 lines as summary
    QStringList lines = output.split(QLatin1Char('\n'));
    QString summary;
    if (lines.size() > 50)
    {
        summary = lines.mid(lines.size() - 50).join(QLatin1Char('\n'));
    }
    else
    {
        summary = output;
    }

    metadata[QStringLiteral("exitCode")] = exitCode;
    return act::core::ToolResult::ok(summary, metadata);
}

act::core::PermissionLevel BuildTool::permissionLevel() const
{
    return act::core::PermissionLevel::Exec;
}

bool BuildTool::isThreadSafe() const
{
    return false;
}

void BuildTool::setTimeoutMs(int ms)
{
    m_timeoutMs = ms;
}

QJsonObject BuildTool::parseBuildOutput(const QString &output)
{
    QJsonObject result;
    int errorCount = 0;
    int warningCount = 0;
    QJsonArray errorLines;

    // Match MSVC:  src/file.cpp(42,10): error C2065: ...
    // Match GCC:    src/file.cpp:42:10: error: ...
    // Match LNK:    LNK2019: unresolved external ...
    static const QRegularExpression errorPattern(
        QStringLiteral("\\b(error|warning|fatal error)\\b"
                        "(?:\\s+(C\\d+|LNK\\d+))?"));

    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const auto &line : lines)
    {
        auto match = errorPattern.match(line);
        if (match.hasMatch())
        {
            const QString severity = match.captured(1);
            if (severity == QStringLiteral("error") ||
                severity == QStringLiteral("fatal error"))
            {
                errorCount++;
                errorLines.append(line.trimmed());
            }
            else if (severity == QStringLiteral("warning"))
            {
                warningCount++;
            }
        }
    }

    result[QStringLiteral("errorCount")] = errorCount;
    result[QStringLiteral("warningCount")] = warningCount;
    if (!errorLines.isEmpty())
        result[QStringLiteral("errors")] = errorLines;

    return result;
}

} // namespace act::harness
