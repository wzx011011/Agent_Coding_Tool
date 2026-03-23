#include "harness/context_manager.h"

#include <spdlog/spdlog.h>

namespace act::harness
{

int ContextManager::estimateTokens(
    const QList<act::core::LLMMessage> &messages) const
{
    // Improved heuristic: base chars + per-message overhead
    // This accounts for JSON formatting overhead in the API request.
    long long totalChars = 0;

    for (const auto &msg : messages)
    {
        // Base content
        totalChars += msg.content.length();

        // Role overhead (system/user/assistant/tool labels in JSON)
        totalChars += 20;

        // Tool call overhead: name, id, JSON params
        if (!msg.toolCall.id.isEmpty() || !msg.toolCall.name.isEmpty())
        {
            totalChars += msg.toolCall.name.length() + msg.toolCall.id.length();
            // Params are JSON — estimate ~20% overhead on string length
            totalChars += 50;
        }

        // tool_callId overhead
        if (!msg.toolCallId.isEmpty())
            totalChars += msg.toolCallId.length() + 20;
    }

    // Divide by 3.0 for English text (GPT-4/Claude avg)
    // The 3.0 factor is tighter than the initial 3.5 but more accurate
    return static_cast<int>(totalChars / 3.0);
}

QList<act::core::LLMMessage> ContextManager::microCompact(
    const QList<act::core::LLMMessage> &messages,
    int targetTokens)
{
    if (messages.isEmpty())
        return messages;

    // Always keep the system message (first message)
    int start = 0;
    if (messages.first().role == act::core::MessageRole::System)
        start = 1;

    // Remove oldest messages from the beginning until we're under budget
    QList<act::core::LLMMessage> result;
    if (start > 0)
        result.append(messages.first()); // Keep system message

    int keptTokens = 0;
    if (start > 0)
        keptTokens = estimateTokens(result);

    // Calculate how many messages we need to remove from the front
    // to get under targetTokens. We iterate from the end to preserve
    // the most recent context.
    for (int i = messages.size() - 1; i >= start; --i)
    {
        int msgTokens = estimateTokens({messages[i]});
        if (keptTokens + msgTokens > targetTokens)
        {
            // We've exceeded the budget — record what was removed
            int removedCount = i - start;
            if (removedCount > 0)
            {
                m_lastCompactSummary = QStringLiteral(
                    "Micro-compact: removed %1 oldest messages "
                    "(approx %2 tokens freed)")
                    .arg(removedCount)
                    .arg(estimateTokens(messages.mid(start, removedCount)));
            }
            break;
        }

        result.prepend(messages[i]);
        keptTokens += msgTokens;
    }

    if (m_lastCompactSummary.isEmpty())
    {
        m_lastCompactSummary = QStringLiteral(
            "Micro-compact: %1 messages remaining, %2 tokens estimated")
            .arg(result.size())
            .arg(keptTokens);
    }

    spdlog::info("{}", m_lastCompactSummary.toStdString());
    return result;
}

} // namespace act::harness
