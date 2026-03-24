#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

#ifdef _WIN32
#include <cstdio>
#include <io.h>
#include <windows.h>
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "core/runtime_version.h"
#include "framework/cli_repl.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/ai_engine.h"
#include "services/config_manager.h"

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // --- Windows UTF-8 bootstrap ---
    // Console output: tell the console to render UTF-8 bytes.
    if (_isatty(_fileno(stdout)))
        SetConsoleOutputCP(CP_UTF8);
    // Console input: reopen stdin so the CRT converts from the console
    // input codepage to UTF-8.  Pipe input is expected to be UTF-8 already
    // (Git Bash, PowerShell 7+, Windows Terminal).
    if (_isatty(_fileno(stdin)))
        freopen(nullptr, "r, ccs=UTF-8", stdin);
#endif

    // Reinitialize spdlog default logger with UTF-8 console sink.
    // Created after SetConsoleOutputCP so the sink inherits UTF-8 codepage.
    {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>("", std::move(sink));
        spdlog::set_default_logger(std::move(logger));
    }

    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("aictl"));
    app.setApplicationVersion(act::core::runtimeVersion());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("ACT CLI - AI Coding Tool runtime"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption jsonOption(
        QStringList{QStringLiteral("j"), QStringLiteral("json")},
        QStringLiteral("Output events as JSON Lines (one JSON object per line)."));
    parser.addOption(jsonOption);

    QCommandLineOption batchOption(
        QStringList{QStringLiteral("b"), QStringLiteral("batch")},
        QStringLiteral("Batch mode: process arguments as inputs and exit."));
    parser.addOption(batchOption);

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

    if (!config->load())
    {
        spdlog::error("Failed to load config from {}. Create .act/config.toml with your credentials.",
                      config->configFilePath().toStdString());
        return 1;
    }

    if (config->apiKey(config->provider()).isEmpty())
    {
        spdlog::error("No API key configured for provider '{}'. Set [api_keys.{}] in .act/config.toml.",
                      config->provider().toStdString(), config->provider().toStdString());
        return 1;
    }

    auto engine = std::make_unique<act::services::AIEngine>(*config);
    auto registry = std::make_unique<act::harness::ToolRegistry>();
    auto permissions = std::make_unique<act::harness::PermissionManager>();
    auto context = std::make_unique<act::harness::ContextManager>();

    permissions->setAutoApproved(act::core::PermissionLevel::Read, true);
    permissions->setAutoApproved(act::core::PermissionLevel::Write, true);
    permissions->setAutoApproved(act::core::PermissionLevel::Exec, true);

    {
        QList<QJsonObject> toolDefs;
        for (const auto &toolName : registry->listTools())
        {
            auto *tool = registry->getTool(toolName);
            if (tool)
            {
                toolDefs.append(QJsonObject{
                    {QStringLiteral("name"), tool->name()},
                    {QStringLiteral("description"), tool->description()},
                    {QStringLiteral("schema"), tool->schema()}
                });
            }
        }
        engine->setToolDefinitions(toolDefs);
    }

    // --- Create CLI REPL ---
    act::framework::CliRepl repl(*engine, *registry, *permissions, *context);

    QTextStream out(stdout);
    QTextStream in(stdin);
    out.setEncoding(QStringConverter::Utf8);
    in.setEncoding(QStringConverter::Utf8);

    if (jsonMode)
        repl.setOutputMode(act::framework::CliRepl::OutputMode::Json);

    QObject::connect(&repl, &act::framework::CliRepl::outputLine,
                     [&out](const QString &line) { out << line << Qt::endl; });
    QObject::connect(&repl, &act::framework::CliRepl::jsonEvent,
                     [&out](const QString &line) { out << line << Qt::endl; });
    QObject::connect(&repl, &act::framework::CliRepl::exitRequested,
                     &app, &QCoreApplication::quit);

    if (!jsonMode)
    {
        QObject::connect(engine.get(), &act::services::AIEngine::streamTokenReceived,
                         [&out](const QString &token) {
                             out << token;
                             out.flush();
                         });
    }

    if (batchMode)
    {
        repl.processBatch(inputs);
        return 0;
    }

    // --- Interactive REPL ---
    out << "ACT CLI v" << app.applicationVersion() << Qt::endl;
    out << "Type your request below. /exit to quit, /reset to clear." << Qt::endl;
    out << Qt::endl;

    while (true)
    {
        out.flush();
        QString line = in.readLine();
        if (line.isNull())
            break;

        repl.processInput(line);
    }

    return 0;
}
