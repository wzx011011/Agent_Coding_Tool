#pragma once

#include <functional>
#include <QList>
#include <QString>
#include <QStringList>

namespace act::infrastructure { class IProcess; }
namespace act::framework { class CommandRegistry; }
namespace act::core { struct LLMMessage; }

namespace act::framework::commands {

/// Copy last LLM response to system clipboard (/copy).
/// Falls back to platform-specific clipboard commands when QGuiApplication
/// is not available.
class CopyCommand
{
public:
    using OutputCallback = std::function<void(const QString &)>;
    using HistoryGetter = std::function<const QList<act::core::LLMMessage> &()>;

    /// Register /copy with the given CommandRegistry.
    /// historyGetter: callable returning const ref to conversation message list.
    static void registerTo(CommandRegistry &registry,
                           infrastructure::IProcess &process,
                           OutputCallback output,
                           HistoryGetter historyGetter);
};

} // namespace act::framework::commands
