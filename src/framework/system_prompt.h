#pragma once

#include <QString>

namespace act::framework
{

/// Built-in base system prompt (ACT identity + general coding rules).
[[nodiscard]] QString defaultBasePrompt();

/// Default project prompt template written by /init command.
[[nodiscard]] QString defaultProjectPromptTemplate();

} // namespace act::framework
