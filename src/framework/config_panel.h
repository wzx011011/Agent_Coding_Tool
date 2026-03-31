#pragma once

#include <QString>
#include <QStringList>

namespace act::services { class ConfigManager; }

namespace act::framework
{

/// Interactive TUI settings panel.
/// Displays configuration items with keyboard navigation (Up/Down/Enter/Esc).
/// Pure static functions — no QObject inheritance, called synchronously from handleConfigCommand.
struct ConfigPanel
{
    struct Item
    {
        QString label;                 // Display name, e.g. "Provider"
        QString configKey;             // Internal key for ConfigManager setters
        QString value;                 // Current display value
        enum Type { Boolean, Enum, String } type;
        QStringList options;           // Enum candidates
        int optionIndex = 0;           // Current index in options list
    };

    /// Run the interactive settings panel.
    /// Returns true if any values were modified (and config was saved).
    /// In non-TTY environments, returns false immediately.
    static bool run(services::ConfigManager &config);
};

} // namespace act::framework
