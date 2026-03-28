#include "harness/tools/test_runner_tool.h"

#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

TestRunnerTool::TestRunnerTool(act::infrastructure::IProcess &proc,
                               QString workspaceRoot)
    : m_proc(proc)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString TestRunnerTool::name() const
{
    return QStringLiteral("test_runner");
}

QString TestRunnerTool::description() const
{
    return QStringLiteral("Run project tests using ctest. "
                          "Supports filtering by test name pattern. "
                          "Returns structured pass/fail/skip results. "
                          "Requires exec permission.");
}

QJsonObject TestRunnerTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("filter")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Optional regex filter for test names "
                          "(passed to ctest -R)");
        return obj;
    }();

    props[QStringLiteral("extra_args")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("array");
        obj[QStringLiteral("items")] =
            QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}};
        obj[QStringLiteral("description")] =
            QStringLiteral("Optional additional ctest arguments "
                          "(e.g. [\"-V\"] for verbose output)");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult TestRunnerTool::execute(const QJsonObject &params)
{
    QStringList args;
    args << QStringLiteral("--test-dir") << QStringLiteral("build");
    args << QStringLiteral("--output-on-failure");

    // Optional filter
    auto filterValue = params.value(QStringLiteral("filter"));
    if (filterValue.isString() && !filterValue.toString().isEmpty())
    {
        args << QStringLiteral("-R") << filterValue.toString();
    }

    // Optional extra args
    auto extraArgs = params.value(QStringLiteral("extra_args"));
    if (extraArgs.isArray())
    {
        for (const auto &item : extraArgs.toArray())
        {
            if (item.isString())
                args.append(item.toString());
        }
    }

    QString output;
    int exitCode = -1;

    spdlog::info("TestRunnerTool: running ctest with args: {}",
                 args.join(QStringLiteral(" ")).toStdString());

    m_proc.execute(QStringLiteral("ctest"), args,
        [&output, &exitCode](int code, QString out) {
            exitCode = code;
            output = std::move(out);
        },
        m_timeoutMs);

    QJsonObject metadata;
    auto parseResult = parseTestOutput(output);
    metadata = parseResult;

    if (exitCode == -1)
    {
        return act::core::ToolResult::err(
            act::core::errors::TIMEOUT,
            QStringLiteral("Tests timed out after %1 ms").arg(m_timeoutMs),
            metadata);
    }

    if (exitCode != 0)
    {
        QString summary = output;
        if (summary.length() > 5000)
        {
            summary = summary.left(1000) +
                      QStringLiteral("\n... [truncated] ...\n") +
                      summary.right(3000);
        }
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("Tests failed (exit code %1):\n%2")
                .arg(exitCode)
                .arg(summary),
            metadata);
    }

    metadata[QStringLiteral("exitCode")] = exitCode;
    return act::core::ToolResult::ok(output, metadata);
}

act::core::PermissionLevel TestRunnerTool::permissionLevel() const
{
    return act::core::PermissionLevel::Exec;
}

bool TestRunnerTool::isThreadSafe() const
{
    return false;
}

void TestRunnerTool::setTimeoutMs(int ms)
{
    m_timeoutMs = ms;
}

QJsonObject TestRunnerTool::parseTestOutput(const QString &output)
{
    QJsonObject result;
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    QJsonArray failedTests;

    static const QRegularExpression testLinePattern(
        QStringLiteral("^\\s*(\\d+/\\d+)\\s+Test\\s+#\\d+:\\s+(.+?)\\s+\\.\\.\\."
                       "\\s+(\\*{0,3})(Passed|Failed|Skipped)"));

    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const auto &line : lines)
    {
        auto match = testLinePattern.match(line.trimmed());
        if (match.hasMatch())
        {
            const QString status = match.captured(4);
            const QString testName = match.captured(2).trimmed();

            if (status == QStringLiteral("Passed"))
                passed++;
            else if (status == QStringLiteral("Failed"))
            {
                failed++;
                failedTests.append(testName);
            }
            else if (status == QStringLiteral("Skipped"))
                skipped++;
        }
    }

    result[QStringLiteral("passed")] = passed;
    result[QStringLiteral("failed")] = failed;
    result[QStringLiteral("skipped")] = skipped;
    result[QStringLiteral("total")] = passed + failed + skipped;
    if (!failedTests.isEmpty())
        result[QStringLiteral("failedTests")] = failedTests;

    return result;
}

} // namespace act::harness
