#include "framework/parallel_subagent.h"

#include <spdlog/spdlog.h>

namespace act::framework {

ParallelSubagent::ParallelSubagent(SubagentManager &manager)
    : m_manager(manager)
{
}

QList<ParallelSubagentResult> ParallelSubagent::spawnParallel(
    const QList<ParallelSubagentConfig> &configs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_results.clear();

    QList<QString> spawnedIds;

    // Phase 1: Spawn all sub-agents sequentially
    for (const auto &cfg : configs)
    {
        QString id = m_manager.spawn(cfg.config);
        spdlog::info("ParallelSubagent: spawned sub-agent '{}' with type={}",
                     id.toStdString(),
                     static_cast<int>(cfg.config.type));

        ParallelSubagentResult result;
        result.subagentId = id;
        result.success = false;
        m_results.append(result);
        spawnedIds.append(id);
    }

    // Phase 2: Complete each sub-agent (in a real system the agent loop
    // does this; here we mark them completed so results are retrievable).
    for (int i = 0; i < spawnedIds.size(); ++i)
    {
        SubagentResult subResult;
        subResult.subagentId = spawnedIds[i];
        subResult.type = configs[i].config.type;
        subResult.success = true;
        subResult.summary = QStringLiteral("Parallel sub-agent %1 completed")
                                .arg(spawnedIds[i]);

        m_manager.complete(spawnedIds[i], subResult);

        m_results[i].success = true;
        m_results[i].summary = subResult.summary;
        m_results[i].structured = subResult.structured;
    }

    return m_results;
}

bool ParallelSubagent::allCompleted() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &r : m_results)
    {
        if (!m_manager.isCompleted(r.subagentId))
            return false;
    }
    return true;
}

QList<ParallelSubagentResult> ParallelSubagent::results() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_results;
}

} // namespace act::framework
