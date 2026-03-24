#pragma once

#include <functional>
#include <QList>
#include <QString>
#include <QStringList>

namespace act::framework
{

/// Information about a registered slash command.
struct CommandInfo
{
    QString name;           // e.g., "help", "reset", "quit"
    QString description;    // One-line help text
    std::function<bool(const QStringList &)> handler;  // Returns true if handled
};

/// Registry for slash commands.
/// Supports registration, unregistration, dispatch, and listing.
class CommandRegistry
{
public:
    CommandRegistry() = default;
    ~CommandRegistry() = default;

    // Non-copyable, movable
    CommandRegistry(const CommandRegistry &) = delete;
    CommandRegistry &operator=(const CommandRegistry &) = delete;
    CommandRegistry(CommandRegistry &&) = default;
    CommandRegistry &operator=(CommandRegistry &&) = default;

    /// Register a command with name, description, and handler.
    /// Returns true on success, false if command already exists.
    [[nodiscard]] bool registerCommand(const QString &name,
                                        const QString &description,
                                        std::function<bool(const QStringList &)> handler);

    /// Unregister a command by name.
    /// Returns true if found and removed, false otherwise.
    [[nodiscard]] bool unregisterCommand(const QString &name);

    /// Execute a command by name with arguments.
    /// Returns true if command was found and executed, false otherwise.
    [[nodiscard]] bool execute(const QString &name, const QStringList &args) const;

    /// Check if a command exists.
    [[nodiscard]] bool hasCommand(const QString &name) const;

    /// Get command info by name. Returns nullptr if not found.
    [[nodiscard]] const CommandInfo *getCommand(const QString &name) const;

    /// List all registered commands.
    [[nodiscard]] QList<CommandInfo> listCommands() const;

    /// Get number of registered commands.
    [[nodiscard]] int commandCount() const { return static_cast<int>(m_commands.size()); }

private:
    QList<CommandInfo> m_commands;

    [[nodiscard]] int findCommandIndex(const QString &name) const;
};

} // namespace act::framework
