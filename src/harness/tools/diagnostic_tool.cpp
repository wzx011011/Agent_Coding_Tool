#include "harness/tools/diagnostic_tool.h"

#include <QJsonArray>
#include <QRegularExpression>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

QString DiagnosticTool::name() const
{
    return QStringLiteral("diagnostic");
}

QString DiagnosticTool::description() const
{
    return QStringLiteral(
        "Parse raw compiler output into structured JSON diagnostics. "
        "Supports MSVC, GCC, and Clang error/warning/note formats.");
}

QJsonObject DiagnosticTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("output")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Raw compiler output text to parse");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{
        QStringLiteral("output")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult DiagnosticTool::execute(const QJsonObject &params)
{
    if (!params.contains(QStringLiteral("output")))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Missing required parameter: output"));
    }

    QString output = params.value(QStringLiteral("output")).toString();
    QJsonArray diagnostics = parseDiagnostics(output);

    int errorCount = 0;
    int warningCount = 0;
    int noteCount = 0;

    for (const auto &diag : diagnostics)
    {
        QString severity =
            diag.toObject().value(QStringLiteral("severity")).toString();
        if (severity == QStringLiteral("error") ||
            severity == QStringLiteral("fatal error"))
        {
            ++errorCount;
        }
        else if (severity == QStringLiteral("warning"))
        {
            ++warningCount;
        }
        else if (severity == QStringLiteral("note"))
        {
            ++noteCount;
        }
    }

    QJsonObject metadata;
    metadata[QStringLiteral("diagnostics")] = diagnostics;
    metadata[QStringLiteral("errorCount")] = errorCount;
    metadata[QStringLiteral("warningCount")] = warningCount;
    metadata[QStringLiteral("noteCount")] = noteCount;

    QString summary =
        QStringLiteral("Parsed %1 diagnostic(s): %2 error(s), "
                       "%3 warning(s), %4 note(s)")
            .arg(diagnostics.size())
            .arg(errorCount)
            .arg(warningCount)
            .arg(noteCount);

    spdlog::debug("DiagnosticTool: {}", summary.toStdString());

    return act::core::ToolResult::ok(summary, metadata);
}

QJsonArray DiagnosticTool::parseDiagnostics(const QString &output)
{
    QJsonArray diagnostics;

    // MSVC format: src/file.cpp(42,10): error C2065: 'x': undeclared
    // identifier
    static const QRegularExpression msvcRegex(
        QStringLiteral("^([^(]+?)\\((\\d+),(\\d+)\\):\\s+"
                       "(error|warning|fatal error)\\s+(\\w+):\\s+(.+)$"));

    // GCC/Clang format: src/file.cpp:42:10: error: 'x' was not declared
    static const QRegularExpression gccRegex(
        QStringLiteral("^([^:]+):(\\d+):(\\d+):\\s+"
                       "(error|warning|fatal error|note):\\s+(.+)$"));

    const auto lines = output.split(QLatin1Char('\n'));
    for (const auto &line : lines)
    {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;

        QJsonObject diag;

        // Try MSVC first
        auto msvcMatch = msvcRegex.match(trimmed);
        if (msvcMatch.hasMatch())
        {
            diag[QStringLiteral("file")] = msvcMatch.captured(1).trimmed();
            diag[QStringLiteral("line")] = msvcMatch.captured(2).toInt();
            diag[QStringLiteral("col")] = msvcMatch.captured(3).toInt();
            diag[QStringLiteral("severity")] = msvcMatch.captured(4);
            diag[QStringLiteral("code")] = msvcMatch.captured(5);
            diag[QStringLiteral("message")] = msvcMatch.captured(6);
            diag[QStringLiteral("raw")] = trimmed;
            diagnostics.append(diag);
            continue;
        }

        // Try GCC/Clang
        auto gccMatch = gccRegex.match(trimmed);
        if (gccMatch.hasMatch())
        {
            diag[QStringLiteral("file")] = gccMatch.captured(1).trimmed();
            diag[QStringLiteral("line")] = gccMatch.captured(2).toInt();
            diag[QStringLiteral("col")] = gccMatch.captured(3).toInt();
            diag[QStringLiteral("severity")] = gccMatch.captured(4);
            diag[QStringLiteral("code")] = QString();
            diag[QStringLiteral("message")] = gccMatch.captured(5);
            diag[QStringLiteral("raw")] = trimmed;
            diagnostics.append(diag);
            continue;
        }

        // Generic fallback: lines containing 'error' or 'warning'
        if (trimmed.contains(QStringLiteral("error")) ||
            trimmed.contains(QStringLiteral("warning")))
        {
            diag[QStringLiteral("file")] = QString();
            diag[QStringLiteral("line")] = 0;
            diag[QStringLiteral("col")] = 0;
            diag[QStringLiteral("severity")] =
                trimmed.contains(QStringLiteral("error"))
                    ? QStringLiteral("error")
                    : QStringLiteral("warning");
            diag[QStringLiteral("code")] = QString();
            diag[QStringLiteral("message")] = trimmed;
            diag[QStringLiteral("raw")] = trimmed;
            diagnostics.append(diag);
        }
    }

    return diagnostics;
}

act::core::PermissionLevel DiagnosticTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool DiagnosticTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
