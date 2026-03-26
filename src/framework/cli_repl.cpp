#include "framework/cli_repl.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <spdlog/spdlog.h>

#include "framework/markdown_formatter.h"
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
{
    // Register built-in commands
    m_commands.registerCommand(
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

    m_commands.registerCommand(
        QStringLiteral("exit"),
        QStringLiteral("Exit the REPL"),
        [this](const QStringList & /*args*/) -> bool {
            m_exitRequested = true;
            emit exitRequested();
            return true;
        });

    m_commands.registerCommand(
        QStringLiteral("quit"),
        QStringLiteral("Exit the REPL (alias for /exit)"),
        [this](const QStringList & /*args*/) -> bool {
            m_exitRequested = true;
            emit exitRequested();
            return true;
        });

    m_commands.registerCommand(
        QStringLiteral("reset"),
        QStringLiteral("Reset conversation context"),
        [this](const QStringList & /*args*/) -> bool {
            emitOutput(TerminalStyle::systemMessage(
                QStringLiteral("Conversation reset.")));
            return true;
        });

    m_commands.registerCommand(
        QStringLiteral("status"),
        QStringLiteral("Show agent loop status"),
        [this](const QStringList & /*args*/) -> bool {
            AgentLoop loop(m_engine, m_tools, m_permissions, m_context, this);
            emitOutput(TerminalStyle::systemMessage(
                QStringLiteral("State: %1, Messages: %2, Turns: %3")
                    .arg(static_cast<int>(loop.state()))
                    .arg(loop.messages().size())
                    .arg(loop.turnCount())));
            return true;
        });

    if (m_modelSwitcher) {
        m_commands.registerCommand(
            QStringLiteral("model"),
            QStringLiteral("Show or switch AI model profile"),
            [this](const QStringList &args) -> bool {
                return handleModelCommand(args);
            });
    }
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
    AgentLoop loop(m_engine, m_tools, m_permissions, m_context, this);
    loop.setMaxTurns(50);

    // Wire up event callback — handle events in both Human and JSON modes
    loop.setEventCallback([this](const act::core::RuntimeEvent &event) {
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

    loop.setFinishCallback([this]() {
        // Nothing extra needed — messages are already emitted via event callback
    });

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
        emitOutput(TerminalStyle::userPrompt(trimmed));
    }

    // Submit and wait for completion
    loop.submitUserMessage(trimmed);

    // Output the conversation messages (skip duplicates from events)
    const auto &messages = loop.messages();
    for (const auto &msg : messages)
    {
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
            // - Only show System messages that weren't already handled
            if (msg.role == act::core::MessageRole::Assistant)
            {
                // Skip — text streamed, tool calls shown via events
                continue;
            }
            if (msg.role == act::core::MessageRole::Tool)
            {
                // Skip — shown via ToolCallCompleted event
                continue;
            }
            emitOutput(formatHumanMessage(msg));
        }
    }

    // Ensure a newline after streaming text
    if (m_outputMode == OutputMode::Human)
    {
        emitOutput(QString());
    }

    // Emit final state
    if (m_outputMode == OutputMode::Json)
    {
        auto finalEvent = act::core::RuntimeEvent::taskState(loop.state());
        emit jsonEvent(formatJsonEvent(finalEvent));
    }
    else if (loop.state() != act::core::TaskState::Completed)
    {
        QString stateStr;
        switch (loop.state())
        {
        case act::core::TaskState::Failed: stateStr = QStringLiteral("failed"); break;
        case act::core::TaskState::Cancelled: stateStr = QStringLiteral("cancelled"); break;
        default: stateStr = QStringLiteral("unknown"); break;
        }
        emitOutput(TerminalStyle::systemMessage(
            QStringLiteral("Agent %1.").arg(stateStr)));
    }

    return loop.state();
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

QString CliRepl::formatHumanEvent(const act::core::RuntimeEvent &event) const
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

        return TerminalStyle::toolCallStarted(name, argsPreview);
    }

    case ET::ToolCallCompleted:
    {
        QString name = event.data.value(QStringLiteral("tool")).toString();
        bool success = event.data.value(QStringLiteral("success")).toBool();
        QString output = event.data.value(QStringLiteral("output")).toString();

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

void CliRepl::emitOutput(const QString &line)
{
    emit outputLine(line);
}

} // namespace act::framework
