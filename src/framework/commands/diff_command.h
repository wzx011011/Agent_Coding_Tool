#pragma once

#include <functional>
#include <QString>
#include <QStringList>

namespace act::infrastructure { class IProcess; }
namespace act::framework { class CommandRegistry; }

namespace act::framework::commands {

/// Show file changes with colored terminal output (/diff).
/// Supports: --staged, --stat, <filepath>, or plain git diff.
class DiffCommand
{
public:
    using OutputCallback = std::function<void(const QString &)>;

    /// Register /diff with the given CommandRegistry.
    static void registerTo(CommandRegistry &registry,
                           infrastructure::IProcess &process,
                           OutputCallback output);
};

} // namespace act::framework::commands
