#include "framework/agent_loop.h"

#include <spdlog/spdlog.h>

namespace act::framework
{

AgentLoop::AgentLoop(services::IAIEngine &engine,
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

void AgentLoop::submitUserMessage(const QString &message)
{
    if (m_state != act::core::TaskState::Idle)
    {
        spdlog::warn("AgentLoop::submitUserMessage called in state {}",
                     static_cast<int>(m_state));
        return;
    }

    act::core::LLMMessage userMsg;
    userMsg.role = act::core::MessageRole::User;
    userMsg.content = message;
    m_messages.append(userMsg);

    m_turnCount = 0;
    m_cancelled = false;
    transitionTo(act::core::TaskState::Running);
    runLoop();
}

void AgentLoop::onPermissionDenied(const QString &toolName,
                                    const QString &reason)
{
    if (m_state != act::core::TaskState::WaitingApproval || !m_pendingToolCall)
        return;

    // Inject a tool result message with the denial
    act::core::ToolResult denied =
        act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Permission denied for %1: %2").arg(toolName, reason));
    appendToolResult(*m_pendingToolCall, denied);
    m_pendingToolCall = std::nullopt;

    transitionTo(act::core::TaskState::Running);
    runLoop();
}

void AgentLoop::onPermissionApproved()
{
    if (m_state != act::core::TaskState::WaitingApproval || !m_pendingToolCall)
        return;

    // Execute the approved tool call
    emitEvent(act::core::RuntimeEvent::toolCall(
        m_pendingToolCall->name, m_pendingToolCall->params));
    auto result = m_tools.execute(m_pendingToolCall->name, m_pendingToolCall->params);
    appendToolResult(*m_pendingToolCall, result);
    m_pendingToolCall = std::nullopt;

    // Continue with next pending tool call or proceed to next loop
    ++m_pendingToolCallIndex;
    transitionTo(act::core::TaskState::ToolRunning);
    dispatchNextPendingToolCall();
}

void AgentLoop::cancel()
{
    if (m_state == act::core::TaskState::Idle ||
        m_state == act::core::TaskState::Completed ||
        m_state == act::core::TaskState::Failed ||
        m_state == act::core::TaskState::Cancelled)
        return;

    m_cancelled = true;
    m_engine.cancel();
    transitionTo(act::core::TaskState::Cancelled);
    if (m_finishCallback)
        m_finishCallback();
}

void AgentLoop::reset()
{
    m_cancelled = true;
    m_engine.cancel();
    m_messages.clear();
    m_turnCount = 0;
    m_pendingToolCall = std::nullopt;
    m_running = false;
    transitionTo(act::core::TaskState::Idle);
}

void AgentLoop::runLoop()
{
    if (m_cancelled)
        return;

    m_running = true;

    // Safety limit
    if (m_turnCount >= m_maxTurns)
    {
        spdlog::warn("AgentLoop: max turns ({}) reached", m_maxTurns);
        act::core::LLMMessage errorMsg;
        errorMsg.role = act::core::MessageRole::Assistant;
        errorMsg.content =
            QStringLiteral("[Agent stopped: maximum turns (%1) reached]")
                .arg(m_maxTurns);
        m_messages.append(errorMsg);
        transitionTo(act::core::TaskState::Failed);
        emitEvent(act::core::RuntimeEvent::error(
            QStringLiteral("MAX_TURNS"),
            QStringLiteral("Exceeded maximum turn limit")));
        m_running = false;
        if (m_finishCallback)
            m_finishCallback();
        return;
    }

    // Context compaction check
    maybeCompactContext();

    // Call AI engine
    m_engine.chat(
        m_messages,
        [this](act::core::LLMMessage msg) { onAIResponse(msg); },
        [this]() { onAIComplete(); },
        [this](QString code, QString msg) { onAIError(code, msg); });
}

void AgentLoop::onAIResponse(const act::core::LLMMessage &msg)
{
    if (m_cancelled)
        return;

    m_messages.append(msg);
    ++m_turnCount;

    // Check if the response contains tool calls (support multiple)
    QList<act::core::ToolCall> allToolCalls;
    if (!msg.toolCalls.isEmpty())
    {
        allToolCalls = msg.toolCalls;
    }
    else if (!msg.toolCall.id.isEmpty() && !msg.toolCall.name.isEmpty())
    {
        allToolCalls.append(msg.toolCall);
    }

    if (!allToolCalls.isEmpty())
    {
        transitionTo(act::core::TaskState::ToolRunning);
        dispatchToolCalls(allToolCalls);
    }
    // If no tool call, the AI gave a final response.
    // onAIComplete() will transition to Completed.
}

void AgentLoop::onAIComplete()
{
    if (m_cancelled)
        return;

    // If we're not in a tool round, this is the final response
    if (m_state == act::core::TaskState::Running)
    {
        transitionTo(act::core::TaskState::Completed);
        m_running = false;
        if (m_finishCallback)
            m_finishCallback();
    }
    // If we were in ToolRunning and just finished dispatching, continue loop
    else if (m_state == act::core::TaskState::ToolRunning)
    {
        transitionTo(act::core::TaskState::Running);
        runLoop();
    }
}

void AgentLoop::onAIError(const QString &errorCode, const QString &errorMessage)
{
    if (m_cancelled)
        return;

    spdlog::error("AgentLoop: AI error [{}]: {}", errorCode.toStdString(),
                  errorMessage.toStdString());

    act::core::LLMMessage errorMsg;
    errorMsg.role = act::core::MessageRole::Assistant;
    errorMsg.content =
        QStringLiteral("[AI Error: %1] %2").arg(errorCode, errorMessage);
    m_messages.append(errorMsg);

    transitionTo(act::core::TaskState::Failed);
    emitEvent(act::core::RuntimeEvent::error(errorCode, errorMessage));
    m_running = false;
    if (m_finishCallback)
        m_finishCallback();
}

void AgentLoop::dispatchToolCall(const act::core::ToolCall &call)
{
    transitionTo(act::core::TaskState::ToolRunning);
    emitEvent(act::core::RuntimeEvent::toolCall(call.name, call.params));

    auto result = m_tools.execute(call.name, call.params);
    appendToolResult(call, result);

    if (!result.success)
    {
        spdlog::warn("Tool '{}' failed: {}", call.name.toStdString(),
                      result.error.toStdString());
    }

    // Continue the loop after tool execution
    transitionTo(act::core::TaskState::Running);
    runLoop();
}

void AgentLoop::dispatchToolCalls(const QList<act::core::ToolCall> &calls)
{
    m_pendingToolCalls = calls;
    m_pendingToolCallIndex = 0;
    m_pendingToolResults.clear();
    dispatchNextPendingToolCall();
}

void AgentLoop::dispatchNextPendingToolCall()
{
    if (m_cancelled)
        return;

    if (m_pendingToolCallIndex >= m_pendingToolCalls.size())
    {
        // All tool calls dispatched — continue the loop
        m_pendingToolCalls.clear();
        m_pendingToolCallIndex = 0;
        transitionTo(act::core::TaskState::Running);
        runLoop();
        return;
    }

    const auto &call = m_pendingToolCalls[m_pendingToolCallIndex];
    transitionTo(act::core::TaskState::ToolRunning);

    auto *tool = m_tools.getTool(call.name);
    if (!tool)
    {
        // Tool not found — append error and move to next
        act::core::ToolResult errResult = act::core::ToolResult::err(
            act::core::errors::TOOL_NOT_FOUND,
            QStringLiteral("Tool '%1' is not registered").arg(call.name));
        appendToolResult(call, errResult);
        ++m_pendingToolCallIndex;
        dispatchNextPendingToolCall();
        return;
    }

    // Check permission
    auto decision = m_permissions.checkPermission(
        tool->permissionLevel(),
        call.name,
        QStringLiteral("Tool call from AI"),
        call.params);

    if (decision == harness::PermissionManager::Decision::Denied)
    {
        act::core::ToolResult deniedResult = act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Permission denied for tool '%1'").arg(call.name));
        appendToolResult(call, deniedResult);
        ++m_pendingToolCallIndex;
        dispatchNextPendingToolCall();
    }
    else if (decision == harness::PermissionManager::Decision::Approved)
    {
        emitEvent(act::core::RuntimeEvent::toolCall(call.name, call.params));
        auto result = m_tools.execute(call.name, call.params);
        appendToolResult(call, result);
        ++m_pendingToolCallIndex;
        dispatchNextPendingToolCall();
    }
    else
    {
        // Needs manual approval — store pending call
        m_pendingToolCall = call;
        transitionTo(act::core::TaskState::WaitingApproval);
        emitEvent(act::core::RuntimeEvent::permissionRequest(
            QStringLiteral("pending"),
            call.name,
            tool->permissionLevel()));
    }
}

void AgentLoop::appendToolResult(const act::core::ToolCall &call,
                                  const act::core::ToolResult &result)
{
    act::core::LLMMessage toolMsg;
    toolMsg.role = act::core::MessageRole::Tool;
    toolMsg.toolCallId = call.id;
    toolMsg.content = result.success
        ? result.output
        : QStringLiteral("Error [%1]: %2").arg(result.errorCode, result.error);
    m_messages.append(toolMsg);
}

void AgentLoop::maybeCompactContext()
{
    int estimated = m_context.estimateTokens(m_messages);
    if (m_context.shouldAutoCompact(estimated))
    {
        int budget = static_cast<int>(m_context.maxContextTokens() * 0.6);
        m_messages = m_context.microCompact(m_messages, budget);
        spdlog::info("Context compacted: {}",
                      m_context.lastCompactSummary().toStdString());
    }
}

void AgentLoop::transitionTo(act::core::TaskState newState)
{
    if (m_state == newState)
        return;

    m_state = newState;
    emitEvent(act::core::RuntimeEvent::taskState(newState));
}

void AgentLoop::emitEvent(const act::core::RuntimeEvent &event)
{
    if (m_eventCallback)
        m_eventCallback(event);
}

AgentLoop::Checkpoint AgentLoop::saveCheckpoint() const
{
    return Checkpoint{m_state, m_messages, m_turnCount};
}

void AgentLoop::restoreCheckpoint(const Checkpoint &cp)
{
    reset();
    m_messages = cp.messages;
    m_turnCount = cp.turnCount;
    transitionTo(cp.state);
}

} // namespace act::framework
