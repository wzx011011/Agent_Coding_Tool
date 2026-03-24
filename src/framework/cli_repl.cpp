#include "framework/cli_repl.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <spdlog/spdlog.h>

#include "framework/markdown_formatter.h"

namespace act::framework
{

CliRepl::CliRepl(services::IAIEngine &engine,
                 harness::ToolRegistry &tools,
                 harness::PermissionManager &permissions,
                 harness::ContextManager &context,
                 QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_tools(tools)
    , m_permissions(permissions)
    , m_context(context)
{
}

act::core::TaskState CliRepl::processInput(const QString &input)
{
    const QString trimmed = input.trimmed();

    if (trimmed.isEmpty())
        return act::core::TaskState::Idle;

    // Handle special commands
    if (trimmed == QLatin1String("/exit") || trimmed == QLatin1String("/quit"))
    {
        emit exitRequested();
        return act::core::TaskState::Idle;
    }

    if (trimmed == QLatin1String("/reset"))
    {
        emitOutput(QStringLiteral("[System] Conversation reset."));
        return act::core::TaskState::Idle;
    }

    if (trimmed == QLatin1String("/status"))
    {
        // Create a temporary loop just to check state
        AgentLoop loop(m_engine, m_tools, m_permissions, m_context, this);
        emitOutput(QStringLiteral("[System] State: %1, Messages: %2, Turns: %3")
                       .arg(static_cast<int>(loop.state()))
                       .arg(loop.messages().size())
                       .arg(loop.turnCount()));
        return act::core::TaskState::Idle;
    }

    // Run the agent loop with the user input
    AgentLoop loop(m_engine, m_tools, m_permissions, m_context, this);
    loop.setMaxTurns(50);

    // Wire up event callback
    loop.setEventCallback([this](const act::core::RuntimeEvent &event) {
        if (m_outputMode == OutputMode::Json)
        {
            emit jsonEvent(formatJsonEvent(event));
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
        emitOutput(QStringLiteral("> %1").arg(trimmed));
    }

    // Submit and wait for completion (AgentLoop runs synchronously with mock engine)
    loop.submitUserMessage(trimmed);

    // Output the conversation messages
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
            // In Human mode, assistant text was already streamed token-by-token.
            // Only emit non-text content (tool calls, errors).
            if (msg.role == act::core::MessageRole::Assistant &&
                !msg.toolCall.id.isEmpty())
            {
                emitOutput(formatHumanMessage(msg));
            }
            else if (msg.role != act::core::MessageRole::Assistant)
            {
                emitOutput(formatHumanMessage(msg));
            }
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
        emitOutput(QStringLiteral("[System] Agent %1.").arg(stateStr));
    }

    return loop.state();
}

void CliRepl::processBatch(const QStringList &inputs)
{
    for (const auto &input : inputs)
    {
        auto state = processInput(input);
        if (state == act::core::TaskState::Idle)
        {
            // Check if exit was requested
            if (input.trimmed() == QLatin1String("/exit") ||
                input.trimmed() == QLatin1String("/quit"))
                return;
        }
    }
}

QString CliRepl::formatHumanMessage(const act::core::LLMMessage &msg) const
{
    switch (msg.role)
    {
    case act::core::MessageRole::System:
        return QStringLiteral("[System] %1").arg(msg.content);
    case act::core::MessageRole::Assistant:
        if (!msg.toolCall.id.isEmpty())
            return QStringLiteral("[Tool Call] %1(%2)")
                .arg(msg.toolCall.name, msg.toolCall.id);
        return MarkdownFormatter::format(msg.content);
    case act::core::MessageRole::Tool:
        return QStringLiteral("[Tool Result] %1").arg(msg.content.left(500));
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

void CliRepl::emitOutput(const QString &line)
{
    emit outputLine(line);
}

} // namespace act::framework
