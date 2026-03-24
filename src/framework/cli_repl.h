#pragma once

#include <QObject>

#include <QString>

#include "framework/agent_loop.h"
#include "framework/command_registry.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/interfaces.h"

namespace act::framework
{

/// CLI Read-Eval-Print Loop.
/// Bridges stdin/stdout with AgentLoop for interactive or batch usage.
class CliRepl : public QObject
{
    Q_OBJECT
public:
    enum class OutputMode
    {
        Human,  // Human-readable formatted output
        Json     // JSON Lines (one JSON object per line)
    };

    explicit CliRepl(services::IAIEngine &engine,
                       harness::ToolRegistry &tools,
                       harness::PermissionManager &permissions,
                       harness::ContextManager &context,
                       QObject *parent = nullptr);

    /// Get the command registry for registering custom commands.
    [[nodiscard]] CommandRegistry &commandRegistry() { return m_commands; }
    [[nodiscard]] const CommandRegistry &commandRegistry() const { return m_commands; }

    /// Set the output mode (human-readable or JSON lines).
    void setOutputMode(OutputMode mode) { m_outputMode = mode; }

    /// Process a single user input and run the agent loop.
    /// Returns the final state of the agent loop.
    [[nodiscard]] act::core::TaskState processInput(const QString &input);

    /// Process a batch of inputs (e.g. from file or args).
    void processBatch(const QStringList &inputs);

    /// Check if exit was requested via /exit or /quit command.
    [[nodiscard]] bool isExitRequested() const { return m_exitRequested; }

    /// Reset the exit requested flag (for reusing the REPL).
    void clearExitRequested() { m_exitRequested = false; }

    /// Current output mode.
    [[nodiscard]] OutputMode outputMode() const { return m_outputMode; }

signals:
    /// Emitted when a line of output is ready to display.
    void outputLine(const QString &line);

    /// Emitted when a JSON event should be emitted (for --json mode).
    void jsonEvent(const QString &jsonLine);

    /// Emitted when the REPL should exit (e.g. /exit command).
    void exitRequested();

private:
    QString formatHumanMessage(const act::core::LLMMessage &msg) const;
    QString formatHumanEvent(const act::core::RuntimeEvent &event) const;
    QString formatJsonMessage(const act::core::LLMMessage &msg) const;
    QString formatJsonEvent(const act::core::RuntimeEvent &event) const;
    void emitOutput(const QString &line);

    services::IAIEngine &m_engine;
    harness::ToolRegistry &m_tools;
    harness::PermissionManager &m_permissions;
    harness::ContextManager &m_context;

    OutputMode m_outputMode = OutputMode::Human;
    CommandRegistry m_commands;
    bool m_exitRequested = false;
};

} // namespace act::framework
