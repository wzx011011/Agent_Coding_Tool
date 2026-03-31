#pragma once

#include <QObject>

#include <QList>

#include <functional>
#include <optional>

#include "core/enums.h"
#include "core/error_codes.h"
#include "core/runtime_event.h"
#include "core/types.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/interfaces.h"

namespace act::framework
{

/// Core agent orchestration loop.
/// Manages the tool_use cycle: send messages → get AI response → dispatch tools → feed results back.
class AgentLoop : public QObject
{
public:
    using EventCallback = std::function<void(const act::core::RuntimeEvent &)>;
    using FinishCallback = std::function<void()>;

    explicit AgentLoop(services::IAIEngine &engine,
                       harness::ToolRegistry &tools,
                       harness::PermissionManager &permissions,
                       harness::ContextManager &context,
                       QObject *parent = nullptr);

    /// Submit a user message and start the agent loop.
    void submitUserMessage(const QString &message);

    /// Resend after permission was denied (injects denial info and continues loop).
    void onPermissionDenied(const QString &toolName, const QString &reason);

    /// Resend after permission was approved (executes the pending tool and continues).
    void onPermissionApproved();

    /// Submit user input when the agent is in WaitingUserInput state.
    void onUserInput(const QString &response);

    /// Current state of the agent loop.
    [[nodiscard]] act::core::TaskState state() const { return m_state; }

    /// Enter Plan Mode: only Read/Network level tools are allowed.
    void enterPlanMode();

    /// Exit Plan Mode: restore all tool access.
    void exitPlanMode();

    /// Check if Plan Mode is currently active.
    [[nodiscard]] bool isPlanMode() const { return m_planMode; }

    /// The full conversation history.
    [[nodiscard]] const QList<act::core::LLMMessage> &messages() const
    {
        return m_messages;
    }

    /// Number of AI turns completed (including tool rounds).
    [[nodiscard]] int turnCount() const { return m_turnCount; }

    /// Cancel the current loop execution.
    void cancel();

    /// Reset to idle state, clearing all messages.
    void reset();

    /// Manually compact the conversation context.
    /// Aggressively summarizes old messages, keeping system prompt and recent context.
    /// Returns the number of messages removed.
    [[nodiscard]] int compact();

    /// Safety limit for maximum turns per user message.
    void setMaxTurns(int maxTurns) { m_maxTurns = maxTurns; }
    [[nodiscard]] int maxTurns() const { return m_maxTurns; }

    /// Set the system prompt to inject before the first user message.
    void setSystemPrompt(const QString &prompt);
    [[nodiscard]] const QString &systemPrompt() const { return m_systemPrompt; }

    /// Set callback for runtime events (state changes, tool calls, etc.).
    void setEventCallback(EventCallback cb) { m_eventCallback = std::move(cb); }

    /// Set callback for loop completion.
    void setFinishCallback(FinishCallback cb) { m_finishCallback = std::move(cb); }

    // --- Checkpoint ---

    struct Checkpoint
    {
        act::core::TaskState state = act::core::TaskState::Idle;
        QList<act::core::LLMMessage> messages;
        int turnCount = 0;
    };

    /// Save current state as a checkpoint.
    [[nodiscard]] Checkpoint saveCheckpoint() const;

    /// Restore from a checkpoint.
    void restoreCheckpoint(const Checkpoint &cp);

private:
    void transitionTo(act::core::TaskState newState);
    void emitEvent(const act::core::RuntimeEvent &event);
    void runLoop();

    /// Called when AIEngine returns a message.
    void onAIResponse(const act::core::LLMMessage &msg);

    /// Called when AIEngine finishes the response.
    void onAIComplete();

    /// Called when AIEngine reports an error.
    void onAIError(const QString &errorCode, const QString &errorMessage);

    /// Dispatch a tool call after permission is granted.
    void dispatchToolCall(const act::core::ToolCall &call);

    /// Dispatch multiple tool calls sequentially.
    void dispatchToolCalls(const QList<act::core::ToolCall> &calls);

    /// Continue dispatching the next pending tool call.
    void dispatchNextPendingToolCall();

    /// Append a tool result message and continue the loop.
    void appendToolResult(const act::core::ToolCall &call,
                          const act::core::ToolResult &result);

    /// Check if context compaction is needed and apply micro-compact.
    void maybeCompactContext();

    services::IAIEngine &m_engine;
    harness::ToolRegistry &m_tools;
    harness::PermissionManager &m_permissions;
    harness::ContextManager &m_context;

    QList<act::core::LLMMessage> m_messages;
    act::core::TaskState m_state = act::core::TaskState::Idle;
    int m_turnCount = 0;
    int m_maxTurns = 50;
    bool m_cancelled = false;
    bool m_running = false;
    QString m_systemPrompt;
    bool m_systemPromptApplied = false;
    bool m_planMode = false;

    /// Pending tool calls during permission check (multiple tool_call support).
    std::optional<act::core::ToolCall> m_pendingToolCall;
    QList<act::core::ToolCall> m_pendingToolCalls;
    int m_pendingToolCallIndex = 0;
    QList<act::core::ToolResult> m_pendingToolResults;

    /// Tool call that triggered WaitingUserInput state.
    std::optional<act::core::ToolCall> m_pendingUserInputCall;

    EventCallback m_eventCallback;
    FinishCallback m_finishCallback;
};

} // namespace act::framework
