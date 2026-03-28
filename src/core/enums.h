#pragma once

namespace act::core
{

enum class PermissionLevel
{
    Read,
    Write,
    Exec,
    Network,
    Destructive
};

enum class TaskState
{
    Idle,
    Running,
    ToolRunning,
    WaitingApproval,
    WaitingUserInput,
    Paused,
    Cancelled,
    Failed,
    Completed
};

enum class MessageRole
{
    System,
    User,
    Assistant,
    Tool
};

} // namespace act::core
