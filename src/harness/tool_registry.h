#pragma once

#include <QJsonObject>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <memory>
#include <unordered_map>

#include "core/types.h"
#include "harness/interfaces.h"

namespace act::harness
{

class ToolRegistry
{
public:
    ToolRegistry() = default;
    ~ToolRegistry() = default;

    ToolRegistry(const ToolRegistry &) = delete;
    ToolRegistry &operator=(const ToolRegistry &) = delete;
    ToolRegistry(ToolRegistry &&) = default;
    ToolRegistry &operator=(ToolRegistry &&) = default;

    // Register a tool (takes ownership)
    void registerTool(std::unique_ptr<ITool> tool);

    // Unregister a tool by name
    void unregisterTool(const QString &name);

    // Look up a tool by name (non-owning pointer, nullptr if not found)
    [[nodiscard]] ITool *getTool(const QString &name) const;

    // List all registered tool names
    [[nodiscard]] QStringList listTools() const;

    // Execute a tool by name with given parameters
    [[nodiscard]] act::core::ToolResult execute(const QString &name,
                                                const QJsonObject &params);

    // Check if a tool is registered
    [[nodiscard]] bool hasTool(const QString &name) const;

    // Number of registered tools
    [[nodiscard]] size_t size() const;

private:
    mutable QMutex m_mutex;
    std::unordered_map<QString, std::unique_ptr<ITool>> m_tools;
};

} // namespace act::harness
