#include "framework/cli_repl.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <spdlog/spdlog.h>

#include "framework/markdown_formatter.h"
#include "framework/system_prompt.h"
#include "framework/terminal_style.h"

namespace act::framework
{

CliRepl::CliRepl(services::IAIEngine &engine,
                 harness::ToolRegistry &tools,
                 harness::PermissionManager &permissions,
                 harness::ContextManager &context,
                 services::IModelSwitcher *modelSwitcher,
                 QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_tools(tools)
    , m_permissions(permissions)
    , m_context(context)
    , m_modelSwitcher(modelSwitcher)
    , m_loop(engine, tools, permissions, context, this)
{
    // Register built-in commands
    (void)m_commands.registerCommand(
        QStringLiteral("help"),
        QStringLiteral("Show available commands"),
        [this](const QStringList & /*args*/) -> bool {
            QString help = TerminalStyle::systemMessage(
                QStringLiteral("Available commands:"));
            emitOutput(help);
            for (const auto &cmd : m_commands.listCommands())
            {
                emitOutput(QStringLiteral("  /%1 - %2")
                    .arg(cmd.name, cmd.description));
            }
            return true;
        });

    (void)m_commands.registerCommand(
        QStringLiteral("exit"),
        QStringLiteral("Exit the REPL"),
        [this](const QStringList & /*args*/) -> bool {
            m_exitRequested = true;
            emit exitRequested();
            return true;
        });

    (void)m_commands.registerCommand(
        QStringLiteral("quit"),
        QStringLiteral("Exit the REPL (alias for /exit)"),
        [this](const QStringList & /*args*/) -> bool {
            m_exitRequested = true;
            emit exitRequested();
            return true;
        });

    (void)m_commands.registerCommand(
        QStringLiteral("reset"),
        QStringLiteral("Reset conversation context"),
        [this](const QStringList & /*args*/) -> bool {
            m_loop.reset();
            m_turnCount = 0;
            emitOutput(TerminalStyle::systemMessage(
                QStringLiteral("Conversation reset.")));
            return true;
        });

    (void)m_commands.registerCommand(
        QStringLiteral("status"),
        QStringLiteral("Show agent loop status"),
        [this](const QStringList & /*args*/) -> bool {
            emitOutput(TerminalStyle::systemMessage(
                QStringLiteral("State: %1, Messages: %2, Turns: %3")
                    .arg(static_cast<int>(m_loop.state()))
                    .arg(m_loop.messages().size())
                    .arg(m_loop.turnCount())));
            return true;
        });

    if (m_modelSwitcher) {
        (void)m_commands.registerCommand(
            QStringLiteral("model"),
            QStringLiteral("Show or switch AI model profile"),
            [this](const QStringList &args) -> bool {
                return handleModelCommand(args);
            });
    }

    (void)m_commands.registerCommand(
        QStringLiteral("verbose"),
        QStringLiteral("Toggle tool output expansion (/v or /v all)"),
        [this](const QStringList &args) -> bool {
            handleVerboseCommand(args);
            return true;
        });

    (void)m_commands.registerCommand(
        QStringLiteral("v"),
        QStringLiteral("Alias for /verbose"),
        [this](const QStringList &args) -> bool {
            handleVerboseCommand(args);
            return true;
        });

    (void)m_commands.registerCommand(
        QStringLiteral("init"),
        QStringLiteral("Initialize .act/system_prompt.md in current project"),
        [this](const QStringList & /*args*/) -> bool {
            QString actDir = QDir::currentPath() + QStringLiteral("/.act");
            QString promptPath = QDir::cleanPath(actDir + QStringLiteral("/system_prompt.md"));

            if (QFile::exists(promptPath)) {
                emitOutput(QStringLiteral(".act/system_prompt.md already exists."));
                return true;
            }

            QDir().mkpath(actDir);
            QFile file(promptPath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                emitOutput(QStringLiteral("Failed to create .act/system_prompt.md"));
                return true;
            }
            file.write(defaultProjectPromptTemplate().toUtf8());
            file.close();
            emitOutput(QStringLiteral(
                "Created .act/system_prompt.md. Edit it to add project-specific instructions."));
            return true;
        });

    // Wire up event callback — handle events in both Human and JSON modes
    m_loop.setEventCallback([this](const act::core::RuntimeEvent &event) {
        // Capture user input prompt for CLI integration
        if (event.type == act::core::EventType::UserInputRequested)
            m_pendingUserPrompt = event.data.value(QStringLiteral("prompt")).toString();

        if (m_outputMode == OutputMode::Json)
        {
            emit jsonEvent(formatJsonEvent(event));
        }
        else
        {
            // In Human mode, format and display key events
            QString human = formatHumanEvent(event);
            if (!human.isEmpty())
                emitOutput(human);
        }
    });

    m_loop.setFinishCallback([this]() {
        // Nothing extra needed — messages are already emitted via event callback
    });
}

act::core::TaskState CliRepl::processInput(const QString &input)
{
    const QString trimmed = input.trimmed();

    if (trimmed.isEmpty())
        return act::core::TaskState::Idle;

    // Check if this is a slash command
    if (trimmed.startsWith(QLatin1Char('/')))
    {
        // Parse command name and arguments
        QString cmdPart = trimmed.mid(1);  // Remove leading '/'
        QStringList parts = cmdPart.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.isEmpty())
            return act::core::TaskState::Idle;

        QString cmdName = parts.takeFirst();
        QStringList args = parts;

        // Try to execute via CommandRegistry
        if (m_commands.execute(cmdName, args))
            return act::core::TaskState::Idle;

        // Unknown command - show error and continue
        emitOutput(TerminalStyle::errorMessage(
            QStringLiteral("UNKNOWN_COMMAND"),
            QStringLiteral("Unknown command: /%1. Type /help for available commands.")
                .arg(cmdName)));
        return act::core::TaskState::Idle;
    }

    // Run the agent loop with the user input

    // Reset per-turn rich output state
    m_toolSections.clear();
    m_sectionIdCounter = 0;
    m_pendingSectionId = -1;

    if (m_outputMode == OutputMode::Json)
    {
        // Emit user message as JSON
        act::core::LLMMessage userMsg;
        userMsg.role = act::core::MessageRole::User;
        userMsg.content = trimmed;
        emit jsonEvent(formatJsonMessage(userMsg));
    }
    else
    {
        // Emit turn separator before each turn (except the first)
        if (m_turnCount > 0)
        {
            emitOutput(TerminalStyle::turnSeparator());
            emitOutput(TerminalStyle::dim(
                QStringLiteral("[context: %1 messages]").arg(m_loop.messages().size())));
        }
        emitOutput(TerminalStyle::userPrompt(trimmed));
        emitOutput(QString()); // Blank line between question and answer
    }

    // Submit and wait for completion
    const int prevMsgCount = m_loop.messages().size();
    m_loop.submitUserMessage(trimmed);

    // Output the conversation messages (skip duplicates from events)
    // In JSON mode, only emit new messages to avoid re-emitting history from
    // previous turns. In Human mode, skip messages already shown via streaming/events.
    const auto &messages = m_loop.messages();
    const int startIdx = (m_outputMode == OutputMode::Json) ? prevMsgCount : 0;
    for (int i = startIdx; i < messages.size(); ++i)
    {
        const auto &msg = messages[i];
        if (msg.role == act::core::MessageRole::User)
            continue; // Already printed the prompt

        if (m_outputMode == OutputMode::Json)
        {
            emit jsonEvent(formatJsonMessage(msg));
        }
        else
        {
            // In Human mode:
            // - Assistant text was already streamed token-by-token (skip)
            // - Tool calls are already shown via ToolCallStarted/Completed events (skip)
            // - System prompt was injected silently (skip)
            if (msg.role == act::core::MessageRole::Assistant)
            {
                continue;
            }
            if (msg.role == act::core::MessageRole::Tool)
            {
                continue;
            }
            if (msg.role == act::core::MessageRole::System)
            {
                continue;
            }
            emitOutput(formatHumanMessage(msg));
        }
    }

    // Handle WaitingUserInput in non-interactive mode
    while (m_loop.state() == act::core::TaskState::WaitingUserInput &&
           m_outputMode != OutputMode::Human)
    {
        spdlog::warn("ask_user triggered in non-interactive mode, auto-responding empty");
        m_loop.onUserInput(QString());
    }

    if (finalizeTurn())
        return act::core::TaskState::WaitingUserInput;

    ++m_turnCount;

    return m_loop.state();
}

void CliRepl::processBatch(const QStringList &inputs)
{
    for (const auto &input : inputs)
    {
        processInput(input);
        if (m_exitRequested)
            return;
    }
}

QString CliRepl::formatHumanEvent(const act::core::RuntimeEvent &event)
{
    using ET = act::core::EventType;

    switch (event.type)
    {
    case ET::ToolCallStarted:
    {
        QString name = event.data.value(QStringLiteral("tool")).toString();
        QJsonObject params = event.data.value(QStringLiteral("params")).toObject();

        // Extract a human-readable preview from params
        QString argsPreview;
        if (params.contains(QStringLiteral("path")))
            argsPreview = QStringLiteral(" ") + params.value(QStringLiteral("path")).toString();
        else if (params.contains(QStringLiteral("pattern")))
            argsPreview = QStringLiteral(" ") + params.value(QStringLiteral("pattern")).toString();
        else if (params.contains(QStringLiteral("command")))
            argsPreview = QStringLiteral(" ") + params.value(QStringLiteral("command")).toString();

        // Record the section for collapsible display
        ToolSection section;
        section.id = m_sectionIdCounter++;
        section.name = name;
        section.args = argsPreview;
        m_toolSections.append(section);
        m_pendingSectionId = section.id;

        return TerminalStyle::toolCallRunning(name, argsPreview);
    }

    case ET::ToolCallCompleted:
    {
        QString name = event.data.value(QStringLiteral("tool")).toString();
        bool success = event.data.value(QStringLiteral("success")).toBool();
        QString output = event.data.value(QStringLiteral("output")).toString();

        // Colorize unified diff output for diff-related tools
        if (success && (name == QLatin1String("diff_view") ||
                        name == QLatin1String("git_diff")))
        {
            output = TerminalStyle::formatDiff(output);
        }

        QString summary;
        if (success)
        {
            // Show line count as summary
            int lineCount = output.count('\n') + (output.isEmpty() ? 0 : 1);
            summary = QStringLiteral("(%1 lines)").arg(lineCount);
        }
        else
        {
            QString code = event.data.value(QStringLiteral("error_code")).toString();
            summary = QStringLiteral("[%1]").arg(code);
        }

        // Update the corresponding section with completion data
        if (m_pendingSectionId >= 0 && !m_toolSections.isEmpty())
        {
            auto &lastSection = m_toolSections.last();
            lastSection.output = output;
            lastSection.summary = summary;
            lastSection.success = success;
            m_pendingSectionId = -1;
        }

        return TerminalStyle::toolCallCompleted(name, summary, success);
    }

    case ET::ErrorOccurred:
    {
        QString code = event.data.value(QStringLiteral("code")).toString();
        QString msg = event.data.value(QStringLiteral("message")).toString();
        return TerminalStyle::errorMessage(code, msg);
    }

    case ET::PermissionRequested:
    {
        QString tool = event.data.value(QStringLiteral("tool")).toString();
        int level = event.data.value(QStringLiteral("level")).toInt();
        QString levelStr;
        switch (static_cast<act::core::PermissionLevel>(level))
        {
        case act::core::PermissionLevel::Read: levelStr = QStringLiteral("Read"); break;
        case act::core::PermissionLevel::Write: levelStr = QStringLiteral("Write"); break;
        case act::core::PermissionLevel::Exec: levelStr = QStringLiteral("Exec"); break;
        case act::core::PermissionLevel::Network: levelStr = QStringLiteral("Network"); break;
        case act::core::PermissionLevel::Destructive: levelStr = QStringLiteral("Destructive"); break;
        }
        return TerminalStyle::permissionRequest(tool, levelStr);
    }

    case ET::TaskStateChanged:
    {
        int stateInt = event.data.value(QStringLiteral("state")).toInt();
        auto state = static_cast<act::core::TaskState>(stateInt);
        // Running = AI is thinking (each turn)
        if (state == act::core::TaskState::Running)
        {
            emit thinkingStarted();
        }
        return {};
    }

    case ET::UserInputRequested:
        return {};

    case ET::UserInputProvided:
        return {};

    default:
        return {};
    }
}

QString CliRepl::formatHumanMessage(const act::core::LLMMessage &msg) const
{
    switch (msg.role)
    {
    case act::core::MessageRole::System:
        return TerminalStyle::systemMessage(msg.content);
    case act::core::MessageRole::Assistant:
        if (!msg.toolCall.id.isEmpty())
            return TerminalStyle::toolCallStarted(
                msg.toolCall.name, QStringLiteral(""));
        return MarkdownFormatter::format(
            msg.content, TerminalStyle::colorEnabled());
    case act::core::MessageRole::Tool:
        return TerminalStyle::toolCallCompleted(
            msg.toolCall.id, msg.content.left(100), true);
    default:
        return msg.content;
    }
}

QString CliRepl::formatJsonMessage(const act::core::LLMMessage &msg) const
{
    QJsonObject obj;
    switch (msg.role)
    {
    case act::core::MessageRole::System:
        obj[QStringLiteral("type")] = QStringLiteral("system");
        break;
    case act::core::MessageRole::User:
        obj[QStringLiteral("type")] = QStringLiteral("user");
        break;
    case act::core::MessageRole::Assistant:
        obj[QStringLiteral("type")] = QStringLiteral("assistant");
        if (!msg.toolCall.id.isEmpty())
        {
            QJsonObject toolCall;
            toolCall[QStringLiteral("id")] = msg.toolCall.id;
            toolCall[QStringLiteral("name")] = msg.toolCall.name;
            toolCall[QStringLiteral("params")] = msg.toolCall.params;
            obj[QStringLiteral("tool_call")] = toolCall;
        }
        break;
    case act::core::MessageRole::Tool:
        obj[QStringLiteral("type")] = QStringLiteral("tool_result");
        obj[QStringLiteral("tool_call_id")] = msg.toolCallId;
        break;
    }
    obj[QStringLiteral("content")] = msg.content;
    return QString::fromUtf8(
        QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QString CliRepl::formatJsonEvent(const act::core::RuntimeEvent &event) const
{
    QJsonObject obj;
    obj[QStringLiteral("event")] = QStringLiteral("runtime_event");
    obj[QStringLiteral("data")] = event.data;
    return QString::fromUtf8(
        QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

bool CliRepl::handleModelCommand(const QStringList &args) {
    if (!m_modelSwitcher)
        return false;

    if (args.isEmpty()) {
        // Show current configuration
        QString profile = m_modelSwitcher->activeProfile();
        emitOutput(TerminalStyle::systemMessage(
            QStringLiteral("Current model configuration:\n"
                           "  Profile:  %1\n"
                           "  Model:    %2\n"
                           "  Provider: %3\n"
                           "  Base URL: %4")
                .arg(profile.isEmpty() ? QStringLiteral("(none)") : profile,
                     m_modelSwitcher->currentModel(),
                     m_modelSwitcher->currentProvider(),
                     m_modelSwitcher->currentBaseUrl())));
        return true;
    }

    if (args.at(0) == QLatin1String("list")) {
        auto profiles = m_modelSwitcher->allProfiles();
        if (profiles.isEmpty()) {
            emitOutput(TerminalStyle::systemMessage(
                QStringLiteral("No profiles configured.")));
            return true;
        }
        QString active = m_modelSwitcher->activeProfile();
        emitOutput(TerminalStyle::systemMessage(
            QStringLiteral("Available profiles:")));
        for (const auto &p : profiles) {
            QString marker = (p.name == active) ? QStringLiteral("* ") : QStringLiteral("  ");
            emitOutput(QStringLiteral("%1%2 - %3 (%4)")
                           .arg(marker, p.name, p.model, p.provider));
        }
        return true;
    }

    // Switch to named profile
    QString profileName = args.at(0);
    if (m_modelSwitcher->profileNames().contains(profileName)) {
        if (m_modelSwitcher->switchToProfile(profileName)) {
            emitOutput(TerminalStyle::systemMessage(
                QStringLiteral("Switched to profile '%1' (model: %2, provider: %3)")
                    .arg(profileName,
                         m_modelSwitcher->currentModel(),
                         m_modelSwitcher->currentProvider())));
        } else {
            emitOutput(TerminalStyle::errorMessage(
                QStringLiteral("SWITCH_FAILED"),
                QStringLiteral("Cannot switch to '%1': check API key configuration.")
                    .arg(profileName)));
        }
    } else {
        emitOutput(TerminalStyle::errorMessage(
            QStringLiteral("PROFILE_NOT_FOUND"),
            QStringLiteral("Unknown profile '%1'.\nAvailable: %2")
                .arg(profileName, m_modelSwitcher->profileNames().join(QStringLiteral(", ")))));
    }
    return true;
}

void CliRepl::respondToUserInput(const QString &response)
{
    // Display user's response
    if (m_outputMode == OutputMode::Human)
    {
        emitOutput(TerminalStyle::userPrompt(response));
        emitOutput(QString());
    }
    else
    {
        act::core::LLMMessage userMsg;
        userMsg.role = act::core::MessageRole::User;
        userMsg.content = response;
        emit jsonEvent(formatJsonMessage(userMsg));
    }

    // Resume the agent loop (synchronous)
    m_loop.onUserInput(response);

    if (finalizeTurn())
        return;

    ++m_turnCount;
}

bool CliRepl::finalizeTurn()
{
    // Ensure a newline after streaming text (Human mode)
    if (m_outputMode == OutputMode::Human)
        emitOutput(QString());

    // JSON mode: emit task state event, caller handles everything
    if (m_outputMode == OutputMode::Json)
    {
        auto finalEvent = act::core::RuntimeEvent::taskState(m_loop.state());
        emit jsonEvent(formatJsonEvent(finalEvent));
        return false;
    }

    // Human mode: handle non-completed states
    if (m_loop.state() == act::core::TaskState::WaitingUserInput)
    {
        emit userInputRequested(m_pendingUserPrompt);
        return true;
    }

    if (m_loop.state() != act::core::TaskState::Completed)
    {
        QString stateStr;
        switch (m_loop.state())
        {
        case act::core::TaskState::Failed: stateStr = QStringLiteral("failed"); break;
        case act::core::TaskState::Cancelled: stateStr = QStringLiteral("cancelled"); break;
        default: stateStr = QStringLiteral("unknown"); break;
        }
        emitOutput(TerminalStyle::systemMessage(
            QStringLiteral("Agent %1.").arg(stateStr)));
    }

    return false;
}

void CliRepl::emitOutput(const QString &line)
{
    emit outputLine(line);
}

void CliRepl::handleVerboseCommand(const QStringList &args)
{
    if (m_toolSections.isEmpty())
    {
        emitOutput(TerminalStyle::systemMessage(
            QStringLiteral("No tool calls to display.")));
        return;
    }

    bool toggleAll = !args.isEmpty() && args.at(0) == QLatin1String("all");

    if (toggleAll)
    {
        // Toggle all sections
        bool anyCollapsed = false;
        for (const auto &s : m_toolSections)
        {
            if (!s.expanded)
            {
                anyCollapsed = true;
                break;
            }
        }
        // If any is collapsed, expand all; otherwise collapse all
        for (auto &s : m_toolSections)
            s.expanded = anyCollapsed;

        emitOutput(TerminalStyle::systemMessage(
            anyCollapsed ? QStringLiteral("Expanded all tool outputs.")
                        : QStringLiteral("Collapsed all tool outputs.")));
    }
    else
    {
        // Toggle the last completed section
        auto &last = m_toolSections.last();
        last.expanded = !last.expanded;
    }

    // Render the expanded sections
    for (const auto &section : m_toolSections)
    {
        if (!section.expanded)
            continue;

        QString header = TerminalStyle::sectionIndicator(false) +
                         QStringLiteral(" ") + section.name + section.args +
                         QStringLiteral(" ") + section.summary;
        emitOutput(header);

        if (!section.output.isEmpty())
        {
            QStringList lines = section.output.split(QLatin1Char('\n'));
            emitOutput(TerminalStyle::resultBox(section.name, lines));
        }
    }
}

} // namespace act::framework
