#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
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
#include "harness/tools/diff_view_tool.h"
#include "harness/tools/file_edit_tool.h"
#include "harness/tools/file_read_tool.h"
#include "harness/tools/file_write_tool.h"
#include "harness/tools/git_commit_tool.h"
#include "harness/tools/git_diff_tool.h"
#include "harness/tools/git_status_tool.h"
#include "harness/tools/glob_tool.h"
#include "harness/tools/grep_tool.h"
#include "harness/tools/repo_map_tool.h"
#include "harness/tools/shell_exec_tool.h"
#include "infrastructure/interfaces.h"
#include "services/ai_engine.h"
#include "services/config_manager.h"

// --- Production IFileSystem implementation ---
class LocalFileSystem : public act::infrastructure::IFileSystem
{
public:
    explicit LocalFileSystem(QString workspaceRoot = QDir::currentPath())
        : m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
    {
    }

    [[nodiscard]] bool readFile(const QString &path, QString &content) const override
    {
        QString fullPath = normalizePath(path);
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        content = QString::fromUtf8(file.readAll());
        return true;
    }

    bool writeFile(const QString &path, const QString &content) override
    {
        QString fullPath = normalizePath(path);
        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        file.write(content.toUtf8());
        return true;
    }

    [[nodiscard]] QStringList listFiles(const QString &dir,
                                        const QString &pattern) const override
    {
        QString fullPath = normalizePath(dir);
        QDir d(fullPath);
        return d.entryList({pattern}, QDir::Files);
    }

    [[nodiscard]] QString normalizePath(const QString &path) const override
    {
        if (QDir::isRelativePath(path))
            return QDir::cleanPath(m_workspaceRoot + QLatin1Char('/') + path);
        return QDir::cleanPath(path);
    }

    [[nodiscard]] bool exists(const QString &path) const override
    {
        return QFile::exists(path) || QDir(path).exists();
    }

    bool removeFile(const QString &path) override
    {
        return QFile::remove(normalizePath(path));
    }

private:
    QString m_workspaceRoot;
};

// --- Production IProcess implementation ---
class LocalProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int timeoutMs = 30000) override
    {
        QProcess process;
        process.start(command, args);
        bool finished = process.waitForFinished(timeoutMs);
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QString error = QString::fromUtf8(process.readAllStandardError());
        int exitCode = finished ? process.exitCode() : -1;
        if (!finished)
        {
            process.kill();
            output += QStringLiteral("\n[Process timed out after %1ms]").arg(timeoutMs);
        }
        if (!error.isEmpty())
            output += QStringLiteral("\n[stderr] ") + error;
        callback(exitCode, output);
    }

    void cancel() override
    {
        // No-op for synchronous implementation
    }
};

// --- Helper: read a line from console with proper encoding ---
static QString readConsoleLine(QTextStream &in)
{
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin != INVALID_HANDLE_VALUE && GetFileType(hStdin) == FILE_TYPE_CHAR)
    {
        wchar_t buffer[4096];
        DWORD charsRead = 0;
        if (ReadConsoleW(hStdin, buffer, 4095, &charsRead, nullptr) && charsRead > 0)
        {
            // Remove trailing CR/LF
            while (charsRead > 0 && (buffer[charsRead - 1] == L'\r' || buffer[charsRead - 1] == L'\n'))
                --charsRead;
            return QString::fromWCharArray(buffer, charsRead);
        }
        return QString(); // EOF
    }
#endif
    return in.readLine();
}

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
    QString providerInput = readConsoleLine(in).trimmed();
    if (providerInput.isEmpty())
        providerInput = QString::fromUtf8(act::services::ConfigManager::DEFAULT_PROVIDER);
    config.setProvider(providerInput);

    // API Key
    out << Qt::endl;
    out << TS::bold(QStringLiteral("  Enter %1 API key (or press Enter to exit):")
                        .arg(providerInput))
        << Qt::endl;
    out << "  > " << Qt::flush;
    QString apiKey = readConsoleLine(in).trimmed();
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
    QString modelInput = readConsoleLine(in).trimmed();
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
    QString urlInput = readConsoleLine(in).trimmed();
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
    // Set console output to UTF-8 for correct display of CJK/emoji.
    // Do NOT set input code page - we use ReadConsoleW directly for
    // reliable UTF-16 input, which bypasses the code page translation.
    SetConsoleOutputCP(CP_UTF8);
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

    // --- Create infrastructure implementations ---
    auto fileSystem = std::make_unique<LocalFileSystem>(QDir::currentPath());
    auto process = std::make_unique<LocalProcess>();

    // --- Register tools ---
    registry->registerTool(std::make_unique<act::harness::FileReadTool>(*fileSystem, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::FileWriteTool>(*fileSystem, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::FileEditTool>(*fileSystem, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::GrepTool>(*fileSystem, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::GlobTool>(*fileSystem, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::ShellExecTool>(*process, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::GitStatusTool>(*process, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::GitDiffTool>(*process, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::GitCommitTool>(*process, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::DiffViewTool>(*process, *fileSystem, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::RepoMapTool>(*fileSystem, *process, QDir::currentPath()));

    permissions->setAutoApproved(act::core::PermissionLevel::Read, true);
    permissions->setAutoApproved(act::core::PermissionLevel::Write, true);
    // Exec remains opt-in -- shell commands require user confirmation

    // --- Permission confirmation callback ---
    // Set up interactive permission confirmation for non-auto-approved levels
    auto permissionCallback = [&out, &in](const act::core::PermissionRequest &request) -> bool {
        namespace TS = act::framework::TerminalStyle;

        // Display permission request
        out << Qt::endl;
        out << TS::fgYellow(QStringLiteral("  Permission Request: %1")
            .arg(request.toolName)) << Qt::endl;
        out << TS::dim(QStringLiteral("  Level: %1 | Description: %2")
            .arg(static_cast<int>(request.level)).arg(request.description)) << Qt::endl;
        out << Qt::endl;
        out << QStringLiteral("  Allow? [y/N/always]: ") << Qt::flush;

        // Read user response using the same helper as main REPL
        QString response = readConsoleLine(in).trimmed().toLower();

        if (response == QLatin1String("always"))
        {
            // TODO: Store "always" preference for this tool/level
            return true;
        }
        else if (response == QLatin1String("y") || response == QLatin1String("yes"))
        {
            return true;
        }
        else
        {
            out << TS::fgRed(QStringLiteral("  Permission denied.")) << Qt::endl;
            return false;
        }
    };
    permissions->setPermissionCallback(permissionCallback);

    {
        QList<QJsonObject> toolDefs;
        for (const auto &toolName : registry->listTools())
        {
            auto *tool = registry->getTool(toolName);
            if (tool)
            {
                // Store raw tool info - providers will convert to their format
                QJsonObject toolDef;
                toolDef[QStringLiteral("name")] = tool->name();
                toolDef[QStringLiteral("description")] = tool->description();
                toolDef[QStringLiteral("schema")] = tool->schema();
                toolDefs.append(toolDef);
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
        QString line = readConsoleLine(in);
        if (line.isNull())
            break;

        (void)repl.processInput(line);
    }

    return 0;
}
