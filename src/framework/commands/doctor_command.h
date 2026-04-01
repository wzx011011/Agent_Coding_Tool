#pragma once

#include <functional>
#include <QString>
#include <QStringList>

namespace act::infrastructure { class IProcess; }
namespace act::framework { class CommandRegistry; }

namespace act::framework::commands {

/// Environment diagnostics slash command (/doctor).
/// Checks toolchain: CMake, Ninja, Qt, MSVC, vcpkg, git, API keys, disk space.
class DoctorCommand
{
public:
    using OutputCallback = std::function<void(const QString &)>;

    /// Register /doctor with the given CommandRegistry.
    /// Uses IProcess to run version-check commands synchronously.
    static void registerTo(CommandRegistry &registry,
                           infrastructure::IProcess &process,
                           OutputCallback output);
};

} // namespace act::framework::commands
