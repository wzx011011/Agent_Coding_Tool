#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>

namespace act::harness
{

enum class HookEventType
{
    PreToolUse,
    PostToolUse,
    UserPromptSubmit,
    Notification,
    SessionStart,
    SessionStop
};

enum class HookCommandType
{
    Shell,  // Run bash command, non-zero exit blocks tool
    Prompt, // Inject text into system prompt
    Http    // POST to URL with event data
};

struct HookCommand
{
    HookCommandType type = HookCommandType::Shell;
    QString command;                    // Shell: command string, Prompt: text, Http: URL
    QString method;                     // Http only: GET/POST (default POST)
    QMap<QString, QString> headers;     // Http only
};

struct HookEntry
{
    HookEventType eventType = HookEventType::PreToolUse;
    QString matcher;                    // Regex pattern to match tool name (empty = match all)
    QList<HookCommand> commands;
    bool enabled = true;
};

struct HookContext
{
    HookEventType eventType = HookEventType::PreToolUse;
    QString toolName;       // For PreToolUse/PostToolUse
    QJsonObject toolParams;
    bool toolSuccess = false;   // For PostToolUse
    QString toolOutput;         // For PostToolUse
    QString userInput;          // For UserPromptSubmit
};

struct HookResult
{
    bool proceed = true;    // false = block tool execution
    QString feedback;       // Text injected into conversation
};

} // namespace act::harness
