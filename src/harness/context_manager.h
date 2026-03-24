#pragma once

#include <QList>

#include "core/types.h"

namespace act::harness
{

/// Manages conversation context with window governance and compression.
class ContextManager
{
public:
    /// Maximum tokens for the context window (provider-specific).
    /// Default: 200000 (Claude's context window).
    [[nodiscard]] int maxContextTokens() const { return m_maxContextTokens; }

    /// Maximum tokens for the compact trigger threshold.
    /// When estimated tokens exceed this, auto-compact triggers.
    [[nodiscard]] int autoCompactThreshold() const
    {
        return static_cast<int>(m_maxContextTokens * 0.8);
    }

    /// Compact threshold for manual compact suggestion.
    [[nodiscard]] int manualCompactThreshold() const
    {
        return static_cast<int>(m_maxContextTokens * 0.9);
    }

    void setMaxContextTokens(int tokens) { m_maxContextTokens = tokens; }

    /// Estimate tokens for a message list using the improved heuristic.
    [[nodiscard]] int estimateTokens(
        const QList<act::core::LLMMessage> &messages) const;

    /// Check if auto-compact should trigger based on current token count.
    [[nodiscard]] bool shouldAutoCompact(
        int estimatedTokens) const
    {
        return estimatedTokens >= autoCompactThreshold();
    }

    /// Check if manual compact is recommended.
    [[nodiscard]] bool shouldSuggestCompact(
        int estimatedTokens) const
    {
        return estimatedTokens >= manualCompactThreshold();
    }

    /// Compact strategy types.
    enum class CompactStrategy
    {
        Micro,   // Remove oldest messages until under threshold
        Auto,    // Summarize and replace oldest messages
        Manual   // User-triggered full compaction
    };

    /// Perform micro-compact: remove oldest messages to fit within budget.
    /// Returns the compacted message list.
    QList<act::core::LLMMessage> microCompact(
        const QList<act::core::LLMMessage> &messages,
        int targetTokens);

    /// Perform auto-compact: triggered automatically when tokens exceed
    /// threshold. Replaces oldest messages with a summary placeholder.
    /// Returns the compacted message list (empty if no compaction needed).
    QList<act::core::LLMMessage> autoCompact(
        const QList<act::core::LLMMessage> &messages);

    /// Perform manual compact: user-triggered aggressive compaction.
    /// Keeps system message, recent messages, and replaces everything
    /// else with a summary placeholder.
    /// Returns the compacted message list.
    QList<act::core::LLMMessage> manualCompact(
        const QList<act::core::LLMMessage> &messages,
        int keepRecentCount = 4);

    /// Returns a summary of the compacted/removed messages for traceability.
    [[nodiscard]] QString lastCompactSummary() const
    {
        return m_lastCompactSummary;
    }

private:
    int m_maxContextTokens = 200000;
    QString m_lastCompactSummary;
};

} // namespace act::harness
