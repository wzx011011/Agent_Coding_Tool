#include "framework/subagent_manager.h"

namespace act::framework
{

QString SubagentManager::spawn(const SubagentConfig &config)
{
    Subagent sub;
    sub.id = generateId();
    sub.config = config;
    sub.completed = false;
    m_subagents.append(std::move(sub));
    return m_subagents.last().id;
}

SubagentResult SubagentManager::result(const QString &subagentId) const
{
    for (const auto &sub : m_subagents)
    {
        if (sub.id == subagentId && sub.completed)
            return sub.result;
    }
    return {};
}

bool SubagentManager::isCompleted(const QString &subagentId) const
{
    for (const auto &sub : m_subagents)
    {
        if (sub.id == subagentId)
            return sub.completed;
    }
    return false;
}

QStringList SubagentManager::listSubagents() const
{
    QStringList ids;
    for (const auto &sub : m_subagents)
        ids.append(sub.id);
    return ids;
}

int SubagentManager::count() const
{
    return m_subagents.size();
}

QStringList SubagentManager::defaultTools(SubagentType type) const
{
    switch (type)
    {
    case SubagentType::Explore:
        return {
            QStringLiteral("file_read"),
            QStringLiteral("grep"),
            QStringLiteral("glob"),
            QStringLiteral("git_status"),
            QStringLiteral("git_diff"),
        };
    case SubagentType::Code:
        // Code sub-agents get all tools
        return {};
    }
    return {};
}

void SubagentManager::setCompletionCallback(CompletionCallback callback)
{
    m_callback = std::move(callback);
}

void SubagentManager::complete(const QString &subagentId,
                                 const SubagentResult &result)
{
    for (auto &sub : m_subagents)
    {
        if (sub.id == subagentId)
        {
            sub.completed = true;
            sub.result = result;
            if (m_callback)
                m_callback(sub.result);
            return;
        }
    }
}

QString SubagentManager::generateId()
{
    return QStringLiteral("sub_%1").arg(++m_counter);
}

} // namespace act::framework
