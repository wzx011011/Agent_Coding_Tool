#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTextStream>
#include <QTimer>

#ifdef _WIN32
#include <cstdio>
#include <io.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <unistd.h>
#endif

#include "core/runtime_version.h"
#include "framework/cli_repl.h"
#include "framework/feishu_channel.h"
#include "framework/interactive_session_controller.h"
#include "framework/input_dispatcher.h"
#include "framework/resume_manager.h"
#include "framework/skill_catalog.h"
#include "framework/skill_loader.h"
#include "framework/subagent_manager.h"
#include "framework/stream_formatter.h"
#include "framework/system_prompt.h"
#include "framework/terminal_style.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "harness/tools/ask_user_tool.h"
#include "harness/tools/diagnostic_tool.h"
#include "harness/tools/todo_write_tool.h"
#include "harness/tools/subagent_tool.h"
#include "harness/tools/skill_tool.h"
#include "harness/tools/diff_view_tool.h"
#include "harness/tools/file_edit_tool.h"
#include "harness/tools/file_delete_tool.h"
#include "harness/tools/directory_tool.h"
#include "harness/tools/build_tool.h"
#include "harness/tools/test_runner_tool.h"
#include "harness/tools/file_read_tool.h"
#include "harness/tools/file_write_tool.h"
#include "harness/tools/git_branch_tool.h"
#include "harness/tools/git_commit_tool.h"
#include "harness/tools/git_diff_tool.h"
#include "harness/tools/git_log_tool.h"
#include "harness/tools/git_status_tool.h"
#include "harness/tools/glob_tool.h"
#include "harness/tools/grep_tool.h"
#include "harness/tools/repo_map_tool.h"
#include "harness/tools/shell_exec_tool.h"
#include "infrastructure/interfaces.h"
#include "services/ai_engine.h"
#include "services/config_manager.h"
#include "tui_app.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

// --- Production IFileSystem implementation ---
class LocalFileSystem : public act::infrastructure::IFileSystem {
  public:
    explicit LocalFileSystem(QString workspaceRoot = QDir::currentPath())
        : m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot))) {}

    [[nodiscard]] bool readFile(const QString &path, QString &content) const override {
        QString fullPath = normalizePath(path);
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        content = QString::fromUtf8(file.readAll());
        return true;
    }

    bool writeFile(const QString &path, const QString &content) override {
        QString fullPath = normalizePath(path);
        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        file.write(content.toUtf8());
        return true;
    }

    [[nodiscard]] QStringList listFiles(const QString &dir, const QString &pattern) const override {
        QString fullPath = normalizePath(dir);
        QDir d(fullPath);
        return d.entryList({pattern}, QDir::Files);
    }

    [[nodiscard]] QString normalizePath(const QString &path) const override {
        if (QDir::isRelativePath(path))
            return QDir::cleanPath(m_workspaceRoot + QLatin1Char('/') + path);
        return QDir::cleanPath(path);
    }

    [[nodiscard]] bool exists(const QString &path) const override { return QFile::exists(path) || QDir(path).exists(); }

    bool removeFile(const QString &path) override { return QFile::remove(normalizePath(path)); }

    bool createDirectory(const QString &path) override { return QDir().mkpath(normalizePath(path)); }

  private:
    QString m_workspaceRoot;
};

// --- Production IProcess implementation ---
class LocalProcess : public act::infrastructure::IProcess {
  public:
    void execute(const QString &command, const QStringList &args, std::function<void(int, QString)> callback,
                 int timeoutMs = 30000) override {
        QProcess process;
        process.start(command, args);

        if (!process.waitForStarted(timeoutMs)) {
            QString output = QStringLiteral("[Failed to start '%1': %2]").arg(command, process.errorString());
#ifdef Q_OS_WIN
            if (command.compare(QStringLiteral("dir"), Qt::CaseInsensitive) == 0) {
                output += QStringLiteral("\n[hint] 'dir' is a cmd.exe built-in on Windows. Use 'cmd' with args ['/c', "
                                         "'dir'] if you need shell semantics.");
            }
#endif
            callback(-2, output);
            return;
        }

        bool finished = process.waitForFinished(timeoutMs);
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QString error = QString::fromUtf8(process.readAllStandardError());
        int exitCode = finished ? process.exitCode() : -1;
        if (!finished) {
            process.kill();
            process.waitForFinished(1000);
            output += QStringLiteral("\n[Process timed out after %1ms]").arg(timeoutMs);
        }
        if (!error.isEmpty())
            output += QStringLiteral("\n[stderr] ") + error;
        callback(exitCode, output);
    }

    void cancel() override {
        // No-op for synchronous implementation
    }
};

// --- Helper: read a line from console with proper encoding ---
static QString readConsoleLine(QTextStream &in) {
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin != INVALID_HANDLE_VALUE && GetFileType(hStdin) == FILE_TYPE_CHAR) {
        wchar_t buffer[4096];
        DWORD charsRead = 0;
        if (ReadConsoleW(hStdin, buffer, 4095, &charsRead, nullptr) && charsRead > 0) {
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

/// Non-blocking stdin check with timeout.
/// Returns the line if input is available, or null QString on timeout/EOF.
static QString readConsoleLineWithTimeout(QTextStream &in, int timeoutMs) {
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin != INVALID_HANDLE_VALUE && GetFileType(hStdin) == FILE_TYPE_CHAR) {
        // Wait for input with timeout
        if (WaitForSingleObject(hStdin, static_cast<DWORD>(timeoutMs)) != WAIT_OBJECT_0)
            return QString();  // Timeout, no input available

        INPUT_RECORD records[256];
        DWORD recordsRead = 0;
        if (!PeekConsoleInputW(hStdin, records, 256, &recordsRead) || recordsRead == 0)
            return QString();

        // Check if there's a key event (not a focus/resize event)
        bool hasKeyEvent = false;
        for (DWORD i = 0; i < recordsRead; ++i) {
            if (records[i].EventType == KEY_EVENT && records[i].Event.KeyEvent.bKeyDown)
                hasKeyEvent = true;
        }
        if (!hasKeyEvent)
            return QString();

        // Data available — use blocking read
        return readConsoleLine(in);
    }
#else
    // Unix: use select() on stdin
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    if (select(1, &fds, nullptr, nullptr, &tv) <= 0)
        return QString();
    return in.readLine();
#endif
    return QString();
}

static bool isInteractiveConsole() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
}

// --- Model profile switcher adapter ---
class ModelSwitcherAdapter : public act::services::IModelSwitcher {
public:
    ModelSwitcherAdapter(act::services::ConfigManager &config,
                         act::services::AIEngine &engine)
        : m_config(config), m_engine(engine) {}

    [[nodiscard]] QStringList profileNames() const override {
        return m_config.profileNames();
    }

    [[nodiscard]] QList<act::services::ModelProfile> allProfiles() const override {
        QList<act::services::ModelProfile> result;
        for (const auto &name : m_config.profileNames()) {
            auto prof = m_config.profile(name);
            if (prof)
                result.append(*prof);
        }
        return result;
    }

    [[nodiscard]] QString activeProfile() const override {
        return m_config.activeProfile();
    }

    [[nodiscard]] QString currentModel() const override {
        return m_config.currentModel();
    }

    [[nodiscard]] QString currentProvider() const override {
        return m_config.provider();
    }

    [[nodiscard]] QString currentBaseUrl() const override {
        return m_config.baseUrl();
    }

    bool switchToProfile(const QString &profileName) override {
        auto prof = m_config.profile(profileName);
        if (!prof)
            return false;

        if (m_config.apiKey(prof->provider).isEmpty())
            return false;

        if (!m_config.setActiveProfile(profileName))
            return false;

        if (!m_engine.reinitializeProvider()) {
            spdlog::error("ModelSwitcher: provider reinitialization failed for profile '{}'",
                          profileName.toStdString());
            return false;
        }
        return true;
    }

private:
    act::services::ConfigManager &m_config;
    act::services::AIEngine &m_engine;
};

// --- First-run interactive setup ---
static bool runFirstTimeSetup(act::services::ConfigManager &config, QTextStream &out, QTextStream &in) {
    namespace TS = act::framework::TerminalStyle;

    out << Qt::endl;
    out << TS::boldCyan(QStringLiteral("  Welcome to ACT CLI v%1!").arg(QCoreApplication::applicationVersion()))
        << Qt::endl;
    out << Qt::endl;
    out << TS::dim(QStringLiteral("  First-time setup -- let's configure your API connection.")) << Qt::endl;
    out << Qt::endl;

    // Provider
    out << TS::bold(QStringLiteral("  Select provider [anthropic/openai_compat]"))
        << " (default: " << act::services::ConfigManager::DEFAULT_PROVIDER << "):" << Qt::endl;
    out << "  > " << Qt::flush;
    QString providerInput = readConsoleLine(in).trimmed();
    if (providerInput.isEmpty())
        providerInput = QString::fromUtf8(act::services::ConfigManager::DEFAULT_PROVIDER);
    config.setProvider(providerInput);

    // API Key
    out << Qt::endl;
    out << TS::bold(QStringLiteral("  Enter %1 API key (or press Enter to exit):").arg(providerInput)) << Qt::endl;
    out << "  > " << Qt::flush;
    QString apiKey = readConsoleLine(in).trimmed();
    if (apiKey.isEmpty()) {
        out << Qt::endl;
        out << TS::fgYellow(QStringLiteral("  No API key provided. Run again when you have your key ready."))
            << Qt::endl;
        return false;
    }
    config.setApiKey(providerInput, apiKey);

    // Model
    out << Qt::endl;
    out << TS::bold(QStringLiteral("  Model")) << " (default: " << act::services::ConfigManager::DEFAULT_MODEL
        << "):" << Qt::endl;
    out << "  > " << Qt::flush;
    QString modelInput = readConsoleLine(in).trimmed();
    if (!modelInput.isEmpty())
        config.setModel(modelInput);
    else
        config.setModel(QString::fromUtf8(act::services::ConfigManager::DEFAULT_MODEL));

    // Base URL
    QString defaultUrl = act::services::ConfigManager::defaultBaseUrl(providerInput);
    out << Qt::endl;
    out << TS::bold(QStringLiteral("  Custom base URL")) << " (default: " << defaultUrl
        << ", press Enter to skip):" << Qt::endl;
    out << "  > " << Qt::flush;
    QString urlInput = readConsoleLine(in).trimmed();
    if (!urlInput.isEmpty())
        config.setBaseUrl(urlInput);

    // Save
    if (!config.save()) {
        out << Qt::endl;
        out << TS::fgRed(QStringLiteral("  Failed to save config.")) << Qt::endl;
        return false;
    }

    out << Qt::endl;
    out << TS::fgGreen(QStringLiteral("  Config saved to %1").arg(config.configFilePath())) << Qt::endl;
    out << Qt::endl;
    return true;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    // --- Windows UTF-8 bootstrap ---
    // Set console output to UTF-8 for correct display of CJK/emoji.
    // Do NOT set input code page - we use ReadConsoleW directly for
    // reliable UTF-16 input, which bypasses the code page translation.
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Reinitialize spdlog default logger with file-only sink.
    // Logs go to act.log in the working directory; console stays clean.
    {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("act.log", true);
        auto logger = std::make_shared<spdlog::logger>("", std::move(sink));
        spdlog::set_default_logger(std::move(logger));
    }

    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("aictl"));
    app.setApplicationVersion(act::core::runtimeVersion());

    // --- Initialize terminal style (VT, TTY detection) ---
    act::framework::TerminalStyle::initialize();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("ACT CLI - AI Coding Tool runtime"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption jsonOption(QStringList{QStringLiteral("j"), QStringLiteral("json")},
                                  QStringLiteral("Output events as JSON Lines (one JSON object per line)."));
    parser.addOption(jsonOption);

    QCommandLineOption batchOption(QStringList{QStringLiteral("b"), QStringLiteral("batch")},
                                   QStringLiteral("Batch mode: process arguments as inputs and exit."));
    parser.addOption(batchOption);

    QCommandLineOption noColorOption(QStringList{QStringLiteral("no-color")},
                                     QStringLiteral("Disable colored output."));
    parser.addOption(noColorOption);

    QCommandLineOption tuiOption(QStringList{QStringLiteral("tui")},
                                 QStringLiteral("Use the experimental fullscreen TUI interface."));
    parser.addOption(tuiOption);

    QCommandLineOption plainOption(QStringList{QStringLiteral("plain")},
                                   QStringLiteral("Use the streaming line-based interactive CLI."));
    parser.addOption(plainOption);

    parser.addPositionalArgument(QStringLiteral("inputs"),
                                 QStringLiteral("Task description or input lines (batch mode)."));
    parser.process(app);

    const QStringList inputs = parser.positionalArguments();
    const bool jsonMode = parser.isSet(jsonOption);
    const bool batchMode = parser.isSet(batchOption) || !inputs.isEmpty();
    const bool noColor = parser.isSet(noColorOption);
    const bool tuiMode = parser.isSet(tuiOption);
    const bool plainMode = parser.isSet(plainOption);
    const bool wantsInteractiveSession = !jsonMode && !batchMode;
    const bool useTuiMode = wantsInteractiveSession && isInteractiveConsole() && tuiMode && !plainMode;

    if (noColor)
        act::framework::TerminalStyle::setColorEnabled(false);

    QTextStream out(stdout);
    QTextStream in(stdin);
    out.setEncoding(QStringConverter::Utf8);
    in.setEncoding(QStringConverter::Utf8);

    if ((plainMode || tuiMode) && (jsonMode || batchMode)) {
        out << act::framework::TerminalStyle::fgRed(
                   QStringLiteral("--plain and --tui are only available in interactive mode."))
            << Qt::endl;
        return 1;
    }

    if (plainMode && tuiMode) {
        out << act::framework::TerminalStyle::fgRed(
                   QStringLiteral("Choose either --plain or --tui, not both."))
            << Qt::endl;
        return 1;
    }

    // --- Create config and load ---
    auto config = std::make_unique<act::services::ConfigManager>();

    if (!config->load()) {
        out << act::framework::TerminalStyle::fgRed(
                   QStringLiteral("Failed to load config from %1.").arg(config->configFilePath()))
            << Qt::endl;
        return 1;
    }

    // --- First-run setup if not configured ---
    if (!config->isConfigured()) {
        if (!runFirstTimeSetup(*config, out, in))
            return 1;

        // Reload to pick up saved config
        if (!config->load() || !config->isConfigured()) {
            out << act::framework::TerminalStyle::fgRed(QStringLiteral("Configuration incomplete. Exiting."))
                << Qt::endl;
            return 1;
        }
    }

    auto engine = std::make_unique<act::services::AIEngine>(*config);
    auto switcher = std::make_unique<ModelSwitcherAdapter>(*config, *engine);
    auto registry = std::make_unique<act::harness::ToolRegistry>();
    auto permissions = std::make_unique<act::harness::PermissionManager>();
    auto context = std::make_unique<act::harness::ContextManager>();

    // --- Create framework managers ---
    auto skillCatalog = std::make_unique<act::framework::SkillCatalog>();
    auto subagentManager = std::make_unique<act::framework::SubagentManager>();

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
    registry->registerTool(std::make_unique<act::harness::GitLogTool>(*process, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::GitBranchTool>(*process, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::DiffViewTool>(*process, *fileSystem, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::RepoMapTool>(*fileSystem, *process, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::FileDeleteTool>(*fileSystem, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::DirectoryTool>(*fileSystem, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::BuildTool>(*process, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::TestRunnerTool>(*process, QDir::currentPath()));
    registry->registerTool(std::make_unique<act::harness::AskUserTool>());
    registry->registerTool(std::make_unique<act::harness::DiagnosticTool>());
    registry->registerTool(std::make_unique<act::harness::TodoWriteTool>());
    registry->registerTool(std::make_unique<act::harness::SubagentTool>(*subagentManager));
    registry->registerTool(std::make_unique<act::harness::SkillTool>(*skillCatalog));

    permissions->setAutoApproved(act::core::PermissionLevel::Read, true);
    permissions->setAutoApproved(act::core::PermissionLevel::Write, true);
    // Exec remains opt-in -- shell commands require user confirmation

    if (!useTuiMode) {
        // --- Permission confirmation callback ---
        // Set up interactive permission confirmation for non-auto-approved levels
        auto permissionCallback = [&out, &in](const act::core::PermissionRequest &request) -> bool {
            namespace TS = act::framework::TerminalStyle;

            QString levelStr;
            switch (request.level) {
            case act::core::PermissionLevel::Read:
                levelStr = QStringLiteral("Read");
                break;
            case act::core::PermissionLevel::Write:
                levelStr = QStringLiteral("Write");
                break;
            case act::core::PermissionLevel::Exec:
                levelStr = QStringLiteral("Exec");
                break;
            case act::core::PermissionLevel::Network:
                levelStr = QStringLiteral("Network");
                break;
            case act::core::PermissionLevel::Destructive:
                levelStr = QStringLiteral("Destructive");
                break;
            }

            out << Qt::endl;
            out << TS::boldMagenta(QStringLiteral("  ? Allow ")) << request.toolName
                << TS::dim(QStringLiteral(" [%1]").arg(levelStr)) << Qt::endl;

            // Show key parameter previews
            if (!request.params.isEmpty()) {
                static const QStringList paramKeys = {
                    QStringLiteral("path"), QStringLiteral("command"),
                    QStringLiteral("pattern"), QStringLiteral("url"),
                    QStringLiteral("file_path")
                };
                for (const auto &key : paramKeys) {
                    if (request.params.contains(key)) {
                        QString val = request.params.value(key).toString();
                        out << TS::dim(QStringLiteral("    %1: "))
                            << TS::fgYellow(val) << Qt::endl;
                    }
                }
            }

            out << TS::dim(QStringLiteral("  Allow? [y/N/always]: ")) << Qt::flush;

            QString response = readConsoleLine(in).trimmed().toLower();

            if (response == QLatin1String("always")) {
                return true;
            }
            if (response == QLatin1String("y") || response == QLatin1String("yes")) {
                return true;
            }

            out << TS::fgRed(QStringLiteral("  Permission denied.")) << Qt::endl;
            return false;
        };
        permissions->setPermissionCallback(permissionCallback);
    }

    {
        QList<QJsonObject> toolDefs;
        for (const auto &toolName : registry->listTools()) {
            auto *tool = registry->getTool(toolName);
            if (tool) {
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
    act::framework::ResumeManager resumeManager;
    act::framework::CliRepl repl(*engine, *registry, *permissions, *context,
                                  switcher.get(), config.get(), &resumeManager);

    // --- Build system prompt ---
    QString systemPrompt;
    {
        // 1) Base prompt (built-in, always present)
        systemPrompt = act::framework::defaultBasePrompt();

        // 2) Project prompt from .act/system_prompt.md
        QString actDir = QDir::currentPath() + QStringLiteral("/.act");
        QString promptPath = QDir::cleanPath(actDir + QStringLiteral("/system_prompt.md"));
        QFile file(promptPath);
        if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            systemPrompt += QStringLiteral("\n\n") + QString::fromUtf8(file.readAll());
            spdlog::info("Loaded project prompt from {}", promptPath.toStdString());
        }

        // 3) TOML skills from .act/skills/
        act::framework::SkillLoader loader;
        int loaded = loader.loadFromDirectory(
            QDir::currentPath() + QStringLiteral("/.act/skills"), *skillCatalog);
        if (loaded > 0) {
            QString skillPrompt = skillCatalog->buildSystemPrompt();
            if (!skillPrompt.isEmpty()) {
                systemPrompt += QStringLiteral("\n\n") + skillPrompt;
            }
        }
    }

    repl.setSystemPrompt(systemPrompt);

    if (jsonMode)
        repl.setOutputMode(act::framework::CliRepl::OutputMode::Json);

    QObject::connect(&repl, &act::framework::CliRepl::jsonEvent,
                     [&out](const QString &line) { out << line << Qt::endl; });
    QObject::connect(&repl, &act::framework::CliRepl::exitRequested, &app, &QCoreApplication::quit);

    // Thinking spinner state and timer
    bool thinking = false;
    int spinnerFrame = 0;
    bool needsNewline = false;  // Set when stream tokens are written directly
    static const QStringList spinnerChars = {
        QStringLiteral("\xe2\xa0\x8b"), QStringLiteral("\xe2\xa0\x99"),
        QStringLiteral("\xe2\xa0\xb9"), QStringLiteral("\xe2\xa0\xb8"),
        QStringLiteral("\xe2\xa0\xbc"), QStringLiteral("\xe2\xa0\xb4"),
        QStringLiteral("\xe2\xa0\xa6"), QStringLiteral("\xe2\xa7\xa7"),
        QStringLiteral("\xe2\xa0\x87"), QStringLiteral("\xe2\xa0\x8f")
    };
    QTimer spinnerTimer;
    spinnerTimer.setInterval(100);

    auto clearSpinnerLine = [&out]() {
        out << act::framework::TerminalStyle::clearLine();
        out.flush();
    };

    QObject::connect(&spinnerTimer, &QTimer::timeout, [&]() {
        out << act::framework::TerminalStyle::clearLine();
        out << act::framework::TerminalStyle::thinkingIndicator(
            spinnerChars[spinnerFrame % spinnerChars.size()]);
        out.flush();
        ++spinnerFrame;
    });

    QObject::connect(&repl, &act::framework::CliRepl::thinkingStarted, [&]() {
        if (jsonMode || useTuiMode || batchMode) return;
        thinking = true;
        spinnerFrame = 0;
        needsNewline = false;
        // Write first frame immediately — don't wait for QTimer
        out << act::framework::TerminalStyle::thinkingIndicator(spinnerChars[0]);
        out.flush();
        spinnerTimer.start();
    });

    // Stream formatter: buffers tokens into lines and applies Markdown formatting
    auto streamFormatter = new act::framework::StreamFormatter(&app);

    if (!jsonMode && !useTuiMode) {
        QObject::connect(engine.get(), &act::services::AIEngine::streamTokenReceived,
                         streamFormatter, &act::framework::StreamFormatter::feedToken);

        QObject::connect(streamFormatter,
                         &act::framework::StreamFormatter::formattedLineReady,
                         [&](const QString &formatted) {
                             if (thinking) {
                                 thinking = false;
                                 spinnerTimer.stop();
                                 clearSpinnerLine();
                             }
                             out << formatted;
                             out.flush();
                             needsNewline = true;
                         });
    }

    QObject::connect(&repl, &act::framework::CliRepl::outputLine,
                     [&out, &needsNewline](const QString &line) {
                         // If stream tokens were written without newline,
                         // ensure tool events start on a new line.
                         if (needsNewline && !line.isEmpty()) {
                             out << Qt::endl;
                         }
                         needsNewline = false;
                         out << line << Qt::endl;
                     });

    // Handle ask_user tool — prompt user and resume the agent loop
    QObject::connect(&repl, &act::framework::CliRepl::userInputRequested,
        [&](const QString &prompt) {
            if (jsonMode || batchMode)
                return;

            // Stop thinking spinner
            if (thinking) {
                thinking = false;
                spinnerTimer.stop();
                clearSpinnerLine();
            }

            out << Qt::endl;
            out << act::framework::TerminalStyle::boldMagenta(QStringLiteral("  ? "))
                << prompt << Qt::endl;
            out << act::framework::TerminalStyle::dim(QStringLiteral("  > ")) << Qt::flush;

            QString response;
            do {
                response = readConsoleLine(in).trimmed();
                if (response.isEmpty())
                {
                    out << act::framework::TerminalStyle::dim(
                               QStringLiteral("  (empty — please try again)"))
                        << Qt::endl;
                    out << act::framework::TerminalStyle::dim(
                               QStringLiteral("  > "))
                        << Qt::flush;
                }
            } while (response.isEmpty());
            repl.respondToUserInput(response);

            // Flush stream formatter and clean up spinner
            streamFormatter->flush();
            if (thinking) {
                thinking = false;
                spinnerTimer.stop();
                clearSpinnerLine();
            }
        });

    if (batchMode) {
        repl.processBatch(inputs);
        return 0;
    }

    if (useTuiMode) {
        act::cli::TuiApp tui(*engine, *registry, *permissions, *context, systemPrompt);
        return tui.run();
    }

    // --- Interactive REPL with channel support ---
    namespace TS = act::framework::TerminalStyle;

    out << TS::boldCyan(QStringLiteral("    _    ____ _____ ")) << Qt::endl;
    out << TS::boldCyan(QStringLiteral("   / \\  / ___|_   _|")) << Qt::endl;
    out << TS::boldCyan(QStringLiteral("  / _ \\| |     | |  ")) << Qt::endl;
    out << TS::boldCyan(QStringLiteral(" / ___ \\ |___  | |  ")) << Qt::endl;
    out << TS::boldCyan(QStringLiteral("/_/   \\_\\____| |_|  ")) << Qt::endl;
    out << Qt::endl;

    QString modeInfo = QStringLiteral("  ACT CLI v%1  |  Streaming mode  |  /exit quit  /reset clear  /model switch  --tui fullscreen")
                           .arg(app.applicationVersion());

    // Feishu channel integration
    std::shared_ptr<act::framework::FeishuChannel> feishuChannel;
    if (config->feishuEnabled()) {
        act::framework::FeishuChannel::Config feishuConfig;
        feishuConfig.appId = config->feishuAppId();
        feishuConfig.appSecret = config->feishuAppSecret();
        feishuConfig.timeoutSeconds = 30;
        feishuConfig.ai = {engine.get(), registry.get(), context.get()};
        feishuConfig.systemPrompt = systemPrompt;
        if (!config->feishuProxy().isEmpty()) {
            int colon = config->feishuProxy().lastIndexOf(QLatin1Char(':'));
            if (colon >= 0) {
                feishuConfig.proxyHost = config->feishuProxy().left(colon);
                feishuConfig.proxyPort = config->feishuProxy().mid(colon + 1).toInt();
            }
        }
        feishuChannel = std::make_shared<act::framework::FeishuChannel>(feishuConfig);
        modeInfo += QStringLiteral("  |  Feishu: connecting...");
    }

    out << TS::dim(modeInfo) << Qt::endl;
    out << Qt::endl;

    // Use the direct CliRepl for stdin (preserving existing behavior)
    // Feishu channel operates independently via its own signals
    if (feishuChannel) {
        feishuChannel->start();
        QObject::connect(feishuChannel.get(), &act::framework::IChannel::statusChanged,
                         [&out](const QString &msg) {
                             out << TS::dim(QStringLiteral("  [Feishu] ")) << msg << Qt::endl;
                         });

        // Incoming Feishu user messages -> display in terminal
        QObject::connect(feishuChannel.get(), &act::framework::IChannel::messageReceived,
                         [&out, &thinking, &spinnerTimer, &clearSpinnerLine](
                             const QString &chatId, const QString &senderId, const QString &text) {
                             if (thinking) {
                                 thinking = false;
                                 spinnerTimer.stop();
                                 clearSpinnerLine();
                             }
                             out << Qt::endl;
                             out << TS::channelUserMessage(QStringLiteral("Feishu"), senderId, text) << Qt::endl;
                             out.flush();
                         });

        // Session created -> connect token streaming and turn completion
        QObject::connect(feishuChannel.get(), &act::framework::IChannel::sessionCreated,
                         [&out](const QString &chatId,
                                act::framework::InteractiveSessionController *controller) {
                             QObject::connect(controller,
                                              &act::framework::InteractiveSessionController::tokenStreamed,
                                              [&out](const QString &token) {
                                                  out << TS::stripAnsi(token);
                                                  out.flush();
                                              });
                             QObject::connect(controller,
                                              &act::framework::InteractiveSessionController::turnCompleted,
                                              [&out]() {
                                                  out << Qt::endl;
                                                  out.flush();
                                              });
                         });
    }

    while (true) {
        out.flush();

        // Process Qt events (Feishu WebSocket signals, timers, etc.)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        QString line = readConsoleLineWithTimeout(in, 100);
        if (line.isNull()) {
            // Check for actual EOF (pipe closed, not just timeout)
#ifdef _WIN32
            HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
            if (hStdin == INVALID_HANDLE_VALUE || GetFileType(hStdin) != FILE_TYPE_CHAR)
                break;
#else
            if (!isatty(STDIN_FILENO))
                break;
#endif
            continue;
        }

        (void)repl.processInput(line);

        // Flush any remaining buffered content from the stream formatter
        streamFormatter->flush();

        // Ensure spinner is cleaned up (e.g. error without tokens)
        if (thinking) {
            thinking = false;
            spinnerTimer.stop();
            clearSpinnerLine();
        }
    }

    if (feishuChannel)
        feishuChannel->stop();

    return 0;
}
