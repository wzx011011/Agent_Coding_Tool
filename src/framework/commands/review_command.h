#pragma once

#include <functional>
#include <QString>
#include <QStringList>

namespace act::infrastructure { class IProcess; }
namespace act::framework { class CommandRegistry; }

namespace act::framework::commands {

/// Structured code review with regex-based static analysis (/review).
/// Scans diff output for common C++/Qt anti-patterns and produces a
/// markdown checklist report.
///
/// Usage:
///   /review                  — review unstaged changes (git diff)
///   /review --staged         — review staged changes (git diff --staged)
///   /review <path>           — review changes in a specific file/dir
class ReviewCommand
{
public:
    using OutputCallback = std::function<void(const QString &)>;

    /// Register /review with the given CommandRegistry.
    static void registerTo(CommandRegistry &registry,
                           infrastructure::IProcess &process,
                           OutputCallback output);

private:
    ReviewCommand() = delete;
};

} // namespace act::framework::commands
