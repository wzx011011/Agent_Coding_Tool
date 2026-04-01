#pragma once

#include <functional>
#include <QString>
#include <QStringList>

namespace act::infrastructure { class IProcess; }
namespace act::framework { class CommandRegistry; }

namespace act::framework::commands {

/// Automated git commit and PR creation workflow (/commit-and-pr).
/// Analyzes staged changes, generates a commit message, commits, and optionally
/// creates a PR via gh CLI.
///
/// Usage:
///   /commit-and-pr           — commit staged changes, attempt PR
///   /commit-and-pr --all     — stage all changes first, then commit + PR
///   /commit-and-pr --draft   — create a draft PR
class CommitPrCommand
{
public:
    using OutputCallback = std::function<void(const QString &)>;

    /// Register /commit-and-pr with the given CommandRegistry.
    static void registerTo(CommandRegistry &registry,
                           infrastructure::IProcess &process,
                           OutputCallback output);

private:
    CommitPrCommand() = delete;
};

} // namespace act::framework::commands
