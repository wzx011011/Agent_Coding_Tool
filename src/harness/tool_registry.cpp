#include "harness/tool_registry.h"

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

void ToolRegistry::registerTool(std::unique_ptr<ITool> tool)
{
    QMutexLocker locker(&m_mutex);

    if (!tool)
    {
        spdlog::warn("ToolRegistry::registerTool called with nullptr");
        return;
    }

    const auto toolName = tool->name();
    m_tools[toolName] = std::move(tool);
    spdlog::info("Tool registered: {}", toolName.toStdString());
}

void ToolRegistry::unregisterTool(const QString &name)
{
    QMutexLocker locker(&m_mutex);

    auto it = m_tools.find(name);
    if (it != m_tools.end())
    {
        m_tools.erase(it);
        spdlog::info("Tool unregistered: {}", name.toStdString());
    }
}

ITool *ToolRegistry::getTool(const QString &name) const
{
    QMutexLocker locker(&m_mutex);

    auto it = m_tools.find(name);
    if (it != m_tools.end())
    {
        return it->second.get();
    }
    return nullptr;
}

QStringList ToolRegistry::listTools() const
{
    QMutexLocker locker(&m_mutex);

    QStringList names;
    names.reserve(static_cast<qsizetype>(m_tools.size()));
    for (const auto &[name, _] : m_tools)
    {
        names.append(name);
    }
    return names;
}

act::core::ToolResult ToolRegistry::execute(const QString &name,
                                       const QJsonObject &params)
{
    ITool *tool = getTool(name);
    if (!tool)
    {
        spdlog::warn("Tool not found: {}", name.toStdString());
        return act::core::ToolResult::err(
            act::core::errors::TOOL_NOT_FOUND,
            QStringLiteral("Tool '%1' is not registered").arg(name));
    }

    spdlog::info("Executing tool: {}", name.toStdString());
    return tool->execute(params);
}

bool ToolRegistry::hasTool(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    return m_tools.contains(name);
}

size_t ToolRegistry::size() const
{
    QMutexLocker locker(&m_mutex);
    return m_tools.size();
}

} // namespace act::harness
