#include "harness/cost_tracker.h"

#include <QJsonArray>

#include <spdlog/spdlog.h>

namespace act::harness
{

// Pricing per 1M tokens: (input, output)
QMap<QString, QPair<double, double>> CostTracker::s_pricing = {
    {QStringLiteral("claude-sonnet-4-6"), {3.00, 15.00}},
    {QStringLiteral("claude-opus-4-6"), {15.00, 75.00}},
    {QStringLiteral("claude-haiku-4-5"), {0.80, 4.00}},
    {QStringLiteral("gpt-4o"), {2.50, 10.00}},
    {QStringLiteral("glm-4-plus"), {1.50, 7.50}},
};

CostTracker::CostTracker() = default;

void CostTracker::recordRequest(
    const QString &model, const TokenUsage &tokens)
{
    double cost = estimateCost(model, tokens);

    CostEntry entry;
    entry.model = model;
    entry.tokens = tokens;
    entry.estimatedCost = cost;
    entry.timestamp = QDateTime::currentDateTime();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.append(entry);
    }

    spdlog::debug("CostTracker: recorded request for model '{}', "
                   "cost=${:.4f}",
                   model.toStdString(), cost);
}

double CostTracker::sessionTotalCost() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    double total = 0.0;
    for (const auto &entry : m_entries)
    {
        total += entry.estimatedCost;
    }
    return total;
}

TokenUsage CostTracker::sessionTotalTokens() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    TokenUsage total;
    for (const auto &entry : m_entries)
    {
        total.inputTokens += entry.tokens.inputTokens;
        total.outputTokens += entry.tokens.outputTokens;
        total.cacheReadTokens += entry.tokens.cacheReadTokens;
        total.cacheWriteTokens += entry.tokens.cacheWriteTokens;
    }
    return total;
}

QList<CostEntry> CostTracker::requestHistory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries;
}

QJsonObject CostTracker::sessionSummary() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    double totalCost = 0.0;
    TokenUsage totalTokens;
    QMap<QString, double> costByModel;

    for (const auto &entry : m_entries)
    {
        totalCost += entry.estimatedCost;
        totalTokens.inputTokens += entry.tokens.inputTokens;
        totalTokens.outputTokens += entry.tokens.outputTokens;
        totalTokens.cacheReadTokens += entry.tokens.cacheReadTokens;
        totalTokens.cacheWriteTokens += entry.tokens.cacheWriteTokens;
        costByModel[entry.model] += entry.estimatedCost;
    }

    QJsonObject summary;
    summary[QStringLiteral("total_requests")] = m_entries.size();
    summary[QStringLiteral("total_cost")] = totalCost;

    QJsonObject tokensObj;
    tokensObj[QStringLiteral("input")] = totalTokens.inputTokens;
    tokensObj[QStringLiteral("output")] = totalTokens.outputTokens;
    tokensObj[QStringLiteral("cache_read")] = totalTokens.cacheReadTokens;
    tokensObj[QStringLiteral("cache_write")] = totalTokens.cacheWriteTokens;
    summary[QStringLiteral("total_tokens")] = tokensObj;

    QJsonObject byModel;
    for (auto it = costByModel.constBegin(); it != costByModel.constEnd();
         ++it)
    {
        byModel[it.key()] = it.value();
    }
    summary[QStringLiteral("cost_by_model")] = byModel;

    return summary;
}

void CostTracker::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

double CostTracker::estimateCost(
    const QString &model, const TokenUsage &tokens)
{
    auto it = s_pricing.constFind(model);
    if (it == s_pricing.constEnd())
    {
        // Unknown model: use a conservative default of $3/$15
        constexpr double kDefaultInput = 3.0;
        constexpr double kDefaultOutput = 15.0;
        const double inputCost =
            static_cast<double>(tokens.inputTokens) * kDefaultInput / 1'000'000.0;
        const double outputCost =
            static_cast<double>(tokens.outputTokens) * kDefaultOutput / 1'000'000.0;
        return inputCost + outputCost;
    }

    const auto [inputPrice, outputPrice] = it.value();
    const double inputCost =
        static_cast<double>(tokens.inputTokens) * inputPrice / 1'000'000.0;
    const double outputCost =
        static_cast<double>(tokens.outputTokens) * outputPrice / 1'000'000.0;
    return inputCost + outputCost;
}

} // namespace act::harness
