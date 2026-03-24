#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include <functional>

#include "core/types.h"

namespace act::services
{

/// Abstract base class for LLM providers (Anthropic, OpenAI-compatible, etc.).
class LLMProvider
{
public:
    virtual ~LLMProvider() = default;

    virtual void setApiKey(const QString &key) = 0;
    virtual void setBaseUrl(const QString &url) = 0;
    virtual void setModel(const QString &model) = 0;
    [[nodiscard]] virtual bool isConfigured() const = 0;
    [[nodiscard]] virtual QString model() const = 0;

    /// Non-streaming chat completion.
    virtual void complete(
        const QList<act::core::LLMMessage> &messages,
        std::function<void(act::core::LLMMessage)> onMessage,
        std::function<void()> onComplete,
        std::function<void(QString, QString)> onError) = 0;

    /// Streaming chat completion.
    virtual void stream(
        const QList<act::core::LLMMessage> &messages,
        std::function<void(QString)> onToken,
        std::function<void(act::core::LLMMessage)> onMessage,
        std::function<void()> onComplete,
        std::function<void(QString, QString)> onError) = 0;

    virtual void cancel() = 0;

    /// Set tool definitions for tool_use support.
    virtual void setToolDefinitions(const QList<QJsonObject> &tools) = 0;
};

} // namespace act::services
