#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace act::framework
{

/// Sub-agent type definition.
enum class SubagentType
{
    Explore, // Read-only tools, search/read/locate
    Code     // Full toolset, implementation tasks
};

/// Result from a completed sub-agent run.
struct SubagentResult
{
    QString subagentId;
    SubagentType type;
    bool success = false;
    QString summary;       // Human-readable summary
    QJsonObject structured; // Structured data (file references, findings, etc.)
};

/// Subagent configuration for spawning a new sub-agent.
struct SubagentConfig
{
    SubagentType type = SubagentType::Explore;
    QString task;           // The task description/prompt
    QStringList allowedTools; // If empty, defaults based on type
    int maxTurns = 20;      // Max agent loop iterations
};

/// Manages sub-agent lifecycle: spawning, isolation, and result collection.
/// Each sub-agent runs with an independent message context and restricted
/// tool set. The main session only receives a structured summary.
class SubagentManager
{
public:
    /// Callback type for when a sub-agent completes.
    using CompletionCallback = std::function<void(const SubagentResult &)>;

    /// Spawn a new sub-agent with the given configuration.
    /// Returns the sub-agent ID.
    [[nodiscard]] QString spawn(const SubagentConfig &config);

    /// Get the result of a completed sub-agent.
    [[nodiscard]] SubagentResult result(const QString &subagentId) const;

    /// Check if a sub-agent has completed.
    [[nodiscard]] bool isCompleted(const QString &subagentId) const;

    /// List all sub-agent IDs.
    [[nodiscard]] QStringList listSubagents() const;

    /// Number of sub-agents created.
    [[nodiscard]] int count() const;

    /// Get default allowed tools for a sub-agent type.
    [[nodiscard]] QStringList defaultTools(SubagentType type) const;

    /// Set a completion callback.
    void setCompletionCallback(CompletionCallback callback);

    /// Complete a sub-agent with its result (called by the agent loop).
    void complete(const QString &subagentId, const SubagentResult &result);

private:
    struct Subagent
    {
        QString id;
        SubagentConfig config;
        bool completed = false;
        SubagentResult result;
    };

    QString generateId();

    QList<Subagent> m_subagents;
    CompletionCallback m_callback;
    int m_counter = 0;
};

} // namespace act::framework
