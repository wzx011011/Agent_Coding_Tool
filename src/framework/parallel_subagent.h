#pragma once

#include <QList>
#include <QJsonObject>
#include <QString>

#include <mutex>

#include "framework/subagent_manager.h"

namespace act::framework {

/// Configuration for spawning a sub-agent within a parallel batch.
struct ParallelSubagentConfig {
    SubagentConfig config;   // Reuse existing SubagentConfig
    bool runInBackground = false;
};

/// Result from a single sub-agent in a parallel batch.
struct ParallelSubagentResult {
    QString subagentId;
    bool success = false;
    QString summary;
    QJsonObject structured;
};

/// Coordination layer over SubagentManager for parallel sub-agent execution.
/// Spawns multiple sub-agents sequentially (SubagentManager handles the agent
/// loop execution) and polls for completion. This avoids the complexity of
/// real threading while still enabling parallel conceptual execution.
class ParallelSubagent {
public:
    explicit ParallelSubagent(SubagentManager &manager);

    /// Spawn multiple sub-agents and collect results.
    /// Each config is passed to SubagentManager::spawn(), then each sub-agent
    /// is completed via SubagentManager::complete() with a synthetic result.
    QList<ParallelSubagentResult> spawnParallel(
        const QList<ParallelSubagentConfig> &configs);

    /// Check if all spawned sub-agents have completed.
    bool allCompleted() const;

    /// Get the results from the last spawnParallel() call.
    QList<ParallelSubagentResult> results() const;

private:
    SubagentManager &m_manager;
    QList<ParallelSubagentResult> m_results;
    mutable std::mutex m_mutex;
};

} // namespace act::framework
