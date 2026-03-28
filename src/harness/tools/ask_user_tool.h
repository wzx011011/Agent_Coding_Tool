#pragma once

#include <QString>

#include "harness/interfaces.h"

namespace act::harness
{

/// Read-only tool that prompts the user for input.
/// Pauses the agent loop until the user responds.
class AskUserTool : public ITool
{
public:
    explicit AskUserTool() = default;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

    /// Set a response callback that provides the user's answer.
    /// The tool execute() is expected to be handled by the AgentLoop
    /// which transitions to WaitingUserInput state and resumes
    /// when the user provides input via onUserInput().
    void setResponseCallback(std::function<void(const QString &)> callback);

    /// Called when the user provides their input.
    /// Returns true if the tool was waiting for input.
    bool onUserInput(const QString &response);

    /// Whether the tool is currently waiting for user input.
    [[nodiscard]] bool isWaiting() const { return m_waiting; }

    /// The prompt that was last requested.
    [[nodiscard]] const QString &pendingPrompt() const { return m_prompt; }

private:
    bool m_waiting = false;
    QString m_prompt;
    std::function<void(const QString &)> m_responseCallback;
};

} // namespace act::harness
