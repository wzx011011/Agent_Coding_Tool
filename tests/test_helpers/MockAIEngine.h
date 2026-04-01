#pragma once

#include "core/types.h"
#include "services/interfaces.h"

#include <QList>
#include <QString>

/// Shared mock AI engine for test files.
/// Provides a response queue — callers enqueue LLMMessage responses,
/// and chat() pops them in FIFO order.
class MockAIEngine : public act::services::IAIEngine
{
public:
    QList<act::core::LLMMessage> responseQueue;
    int callCount = 0;
    QString lastError = QStringLiteral("INTERNAL_ERROR");

    void chat(const QList<act::core::LLMMessage> & /*messages*/,
              std::function<void(act::core::LLMMessage)> onMessage,
              std::function<void()> onComplete,
              std::function<void(QString, QString)> onError) override
    {
        ++callCount;
        if (!responseQueue.isEmpty())
        {
            onMessage(responseQueue.takeFirst());
            onComplete();
        }
        else
        {
            onError(lastError, QStringLiteral("mock error"));
        }
    }

    void cancel() override {}
    void setToolDefinitions(
        const QList<QJsonObject> & /*tools*/) override {}

    [[nodiscard]] int estimateTokens(
        const QList<act::core::LLMMessage> &messages) const override
    {
        int total = 0;
        for (const auto &m : messages)
            total += m.content.length();
        return static_cast<int>(total / 3.0);
    }
};
