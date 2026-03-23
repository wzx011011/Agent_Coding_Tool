#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

#include "core/runtime_version.h"
#include "framework/cli_repl.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/ai_engine.h"
#include "services/config_manager.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("aictl"));
    app.setApplicationVersion(act::core::runtimeVersion());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("ACT CLI — AI Coding Tool runtime"));
    parser.addHelpOption();
    parser.addVersionOption();

    // --json: output JSON Lines instead of human-readable text
    QCommandLineOption jsonOption(
        QStringList{QStringLiteral("j"), QStringLiteral("json")},
        QStringLiteral("Output events as JSON Lines (one JSON object per line)."));
    parser.addOption(jsonOption);

    // --batch: process input from arguments and exit (no interactive REPL)
    QCommandLineOption batchOption(
        QStringList{QStringLiteral("b"), QStringLiteral("batch")},
        QStringLiteral("Batch mode: process arguments as inputs and exit."));
    parser.addOption(batchOption);

    // Positional arguments: task description or input lines
    parser.addPositionalArgument(
        QStringLiteral("inputs"),
        QStringLiteral("Task description or input lines (batch mode)."));
    parser.process(app);

    const QStringList inputs = parser.positionalArguments();
    const bool jsonMode = parser.isSet(jsonOption);
    const bool batchMode = parser.isSet(batchOption) || !inputs.isEmpty();

    // --- Create service instances ---
    auto config = std::make_unique<act::services::ConfigManager>(
        QDir::currentPath());
    auto engine = std::make_unique<act::services::AIEngine>(*config);
    auto registry = std::make_unique<act::harness::ToolRegistry>();
    auto permissions = std::make_unique<act::harness::PermissionManager>();
    auto context = std::make_unique<act::harness::ContextManager>();

    // --- Create CLI REPL ---
    act::framework::CliRepl repl(*engine, *registry, *permissions, *context);

    QTextStream out(stdout);
    QTextStream in(stdin);

    // Set output mode
    if (jsonMode)
        repl.setOutputMode(act::framework::CliRepl::OutputMode::Json);

    // Connect output
    QObject::connect(&repl, &act::framework::CliRepl::outputLine,
                     [&out](const QString &line) { out << line << Qt::endl; });
    QObject::connect(&repl, &act::framework::CliRepl::jsonEvent,
                     [&out](const QString &line) { out << line << Qt::endl; });
    QObject::connect(&repl, &act::framework::CliRepl::exitRequested,
                     &app, &QCoreApplication::quit);

    if (batchMode)
    {
        repl.processBatch(inputs);
        return 0;
    }

    // --- Interactive REPL ---
    out << "ACT CLI v" << app.applicationVersion() << Qt::endl;
    out << "Type your request below. /exit to quit, /reset to clear." << Qt::endl;
    out << Qt::endl;

    // Read lines until EOF or /exit
    while (true)
    {
        out.flush();
        QString line = in.readLine();
        if (line.isNull())
            break; // EOF

        repl.processInput(line);
    }

    return 0;
}
