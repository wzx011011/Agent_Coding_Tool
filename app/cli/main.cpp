#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
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
#include "framework/terminal_style.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/ai_engine.h"
#include "services/config_manager.h"

// --- First-run interactive setup ---
static bool runFirstTimeSetup(act::services::ConfigManager &config,
                              QTextStream &out,
                              QTextStream &in)
{
    namespace TS = act::framework::TerminalStyle;

    out << Qt::endl;
    out << TS::boldCyan(QStringLiteral("  Welcome to ACT CLI v%1!")
                            .arg(QCoreApplication::applicationVersion()))
        << Qt::endl;
    out << Qt::endl;
    out << TS::dim(QStringLiteral("  First-time setup -- let's configure your API connection."))
        << Qt::endl;
    out << Qt::endl;

    // Provider
    out << TS::bold(QStringLiteral("  Select provider [anthropic/openai_compat]"))
        << " (default: " << act::services::ConfigManager::DEFAULT_PROVIDER << "):"
        << Qt::endl;
    out << "  > " << Qt::flush;
    QString providerInput = in.readLine().trimmed();
    if (providerInput.isEmpty())
        providerInput = QString::fromUtf8(act::services::ConfigManager::DEFAULT_PROVIDER);
    config.setProvider(providerInput);

    // API Key
    out << Qt::endl;
    out << TS::bold(QStringLiteral("  Enter %1 API key (or press Enter to exit):")
                        .arg(providerInput))
        << Qt::endl;
    out << "  > " << Qt::flush;
    QString apiKey = in.readLine().trimmed();
    if (apiKey.isEmpty())
    {
        out << Qt::endl;
        out << TS::fgYellow(QStringLiteral(
            "  No API key provided. Run again when you have your key ready."))
            << Qt::endl;
        return false;
    }
    config.setApiKey(providerInput, apiKey);

    // Model
    out << Qt::endl;
    out << TS::bold(QStringLiteral("  Model"))
        << " (default: " << act::services::ConfigManager::DEFAULT_MODEL << "):"
        << Qt::endl;
    out << "  > " << Qt::flush;
    QString modelInput = in.readLine().trimmed();
    if (!modelInput.isEmpty())
        config.setModel(modelInput);
    else
        config.setModel(QString::fromUtf8(act::services::ConfigManager::DEFAULT_MODEL));

    // Base URL
    QString defaultUrl = act::services::ConfigManager::defaultBaseUrl(providerInput);
    out << Qt::endl;
    out << TS::bold(QStringLiteral("  Custom base URL"))
        << " (default: " << defaultUrl << ", press Enter to skip):"
        << Qt::endl;
    out << "  > " << Qt::flush;
    QString urlInput = in.readLine().trimmed();
    if (!urlInput.isEmpty())
        config.setBaseUrl(urlInput);

    // Save
    if (!config.save())
    {
        out << Qt::endl;
        out << TS::fgRed(QStringLiteral("  Failed to save config.")) << Qt::endl;
        return false;
    }

    out << Qt::endl;
    out << TS::fgGreen(QStringLiteral("  Config saved to %1")
                         .arg(config.configFilePath()))
        << Qt::endl;
    out << Qt::endl;
    return true;
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // --- Windows UTF-8 bootstrap ---
    // Set both input and output code pages to UTF-8 so CJK / emoji
    // characters render correctly in every terminal (cmd, PowerShell,
    // Windows Terminal, ConEmu, Git Bash, etc.).
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // Reopen stdin with UTF-8 mode so the CRT converts wide console
    // input to UTF-8 for QTextStream.  Pipe input is already UTF-8.
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

    // --- Initialize terminal style (VT, TTY detection) ---
    act::framework::TerminalStyle::initialize();

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

    QCommandLineOption noColorOption(
        QStringList{QStringLiteral("no-color")},
        QStringLiteral("Disable colored output."));
    parser.addOption(noColorOption);

    parser.addPositionalArgument(
        QStringLiteral("inputs"),
        QStringLiteral("Task description or input lines (batch mode)."));
    parser.process(app);

    const QStringList inputs = parser.positionalArguments();
    const bool jsonMode = parser.isSet(jsonOption);
    const bool batchMode = parser.isSet(batchOption) || !inputs.isEmpty();
    const bool noColor = parser.isSet(noColorOption);

    if (noColor)
        act::framework::TerminalStyle::setColorEnabled(false);

    QTextStream out(stdout);
    QTextStream in(stdin);
    out.setEncoding(QStringConverter::Utf8);
    in.setEncoding(QStringConverter::Utf8);

    // --- Create config and load ---
    auto config = std::make_unique<act::services::ConfigManager>();

    if (!config->load())
    {
        out << act::framework::TerminalStyle::fgRed(
            QStringLiteral("Failed to load config from %1.")
                .arg(config->configFilePath()))
            << Qt::endl;
        return 1;
    }

    // --- First-run setup if not configured ---
    if (!config->isConfigured())
    {
        if (!runFirstTimeSetup(*config, out, in))
            return 1;

        // Reload to pick up saved config
        if (!config->load() || !config->isConfigured())
        {
            out << act::framework::TerminalStyle::fgRed(
                QStringLiteral("Configuration incomplete. Exiting."))
                << Qt::endl;
            return 1;
        }
    }

    auto engine = std::make_unique<act::services::AIEngine>(*config);
    auto registry = std::make_unique<act::harness::ToolRegistry>();
    auto permissions = std::make_unique<act::harness::PermissionManager>();
    auto context = std::make_unique<act::harness::ContextManager>();

    permissions->setAutoApproved(act::core::PermissionLevel::Read, true);
    permissions->setAutoApproved(act::core::PermissionLevel::Write, true);
    // Exec remains opt-in -- shell commands require user confirmation

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

    // --- Interactive REPL: colored banner ---
    namespace TS = act::framework::TerminalStyle;

    out << TS::boldCyan(QStringLiteral("    _    ____ _____ ")) << Qt::endl;
    out << TS::boldCyan(QStringLiteral("   / \\  / ___|_   _|")) << Qt::endl;
    out << TS::boldCyan(QStringLiteral("  / _ \\| |     | |  ")) << Qt::endl;
    out << TS::boldCyan(QStringLiteral(" / ___ \\ |___  | |  ")) << Qt::endl;
    out << TS::boldCyan(QStringLiteral("/_/   \\_\\____| |_|  ")) << Qt::endl;
    out << Qt::endl;

    out << TS::dim(QStringLiteral(
        "  ACT CLI v%1  |  Type your request below  |  /exit to quit, /reset to clear"))
        .arg(app.applicationVersion())
        << Qt::endl;
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
