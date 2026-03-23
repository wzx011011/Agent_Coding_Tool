#pragma once

#include <QJsonObject>
#include <QString>

#include <functional>
#include <list>

#include "core/types.h"

namespace act::services
{

/// Stub Anthropic API provider. HTTP/SSE wiring will be added later
/// when INetwork implementation is available.
class AnthropicProvider
{
public:
    AnthropicProvider() = default;

    void setApiKey(const QString &key);
    void setModel(const QString &model);
    [[nodiscard]] QString model() const;
    [[nodiscard]] bool isConfigured() const;

    /// Non-streaming chat completion (stub).
    void complete(
        const QList<act::core::LLMMessage> &messages,
        std::function<void(act::core::LLMMessage)> onMessage,
        std::function<void()> onComplete,
        std::function<void(QString, QString)> onError);

    /// Streaming chat completion (stub).
    void stream(
        const QList<act::core::LLMMessage> &messages,
        std::function<void(QString)> onToken,
        std::function<void()> onComplete,
        std::function<void(QString, QString)> onError);

    void cancel();

private:
    QString m_apiKey;
    QString m_model;
    bool m_cancelled = false;
};

} // namespace act::services
