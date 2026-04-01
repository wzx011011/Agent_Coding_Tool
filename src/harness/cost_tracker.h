#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QPair>
#include <QString>

#include <mutex>

namespace act::harness
{

struct TokenUsage
{
    int inputTokens = 0;
    int outputTokens = 0;
    int cacheReadTokens = 0;
    int cacheWriteTokens = 0;
};

struct CostEntry
{
    QString model;
    TokenUsage tokens;
    double estimatedCost = 0.0;
    QDateTime timestamp;
};

class CostTracker
{
public:
    CostTracker();

    void recordRequest(const QString &model, const TokenUsage &tokens);
    [[nodiscard]] double sessionTotalCost() const;
    [[nodiscard]] TokenUsage sessionTotalTokens() const;
    [[nodiscard]] QList<CostEntry> requestHistory() const;
    [[nodiscard]] QJsonObject sessionSummary() const;
    void reset();

    /// Estimate cost for a single request based on model pricing.
    /// Pricing is per 1M tokens.
    [[nodiscard]] static double estimateCost(
        const QString &model, const TokenUsage &tokens);

private:
    mutable std::mutex m_mutex;
    QList<CostEntry> m_entries;

    // model -> (input_price_per_1M, output_price_per_1M)
    static QMap<QString, QPair<double, double>> s_pricing;
};

} // namespace act::harness
