#include "framework/command_registry.h"

namespace act::framework
{

bool CommandRegistry::registerCommand(const QString &name,
                                       const QString &description,
                                       std::function<bool(const QStringList &)> handler)
{
    if (name.isEmpty() || !handler)
        return false;

    // Check for duplicate
    if (hasCommand(name))
        return false;

    CommandInfo info;
    info.name = name;
    info.description = description;
    info.handler = std::move(handler);

    m_commands.append(std::move(info));
    return true;
}

bool CommandRegistry::unregisterCommand(const QString &name)
{
    int idx = findCommandIndex(name);
    if (idx < 0)
        return false;

    m_commands.removeAt(idx);
    return true;
}

bool CommandRegistry::execute(const QString &name, const QStringList &args) const
{
    const CommandInfo *cmd = getCommand(name);
    if (!cmd || !cmd->handler)
        return false;

    return cmd->handler(args);
}

bool CommandRegistry::hasCommand(const QString &name) const
{
    return findCommandIndex(name) >= 0;
}

const CommandInfo *CommandRegistry::getCommand(const QString &name) const
{
    int idx = findCommandIndex(name);
    if (idx < 0)
        return nullptr;
    return &m_commands.at(idx);
}

QList<CommandInfo> CommandRegistry::listCommands() const
{
    return m_commands;
}

int CommandRegistry::findCommandIndex(const QString &name) const
{
    for (int i = 0; i < m_commands.size(); ++i)
    {
        if (m_commands.at(i).name.compare(name, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

} // namespace act::framework
