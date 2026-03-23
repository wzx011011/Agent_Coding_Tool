#include "services/anthropic_provider.h"

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::services
{

void AnthropicProvider::setApiKey(const QString &key)
{
    m_apiKey = key;
}

void AnthropicProvider::setModel(const QString &model)
{
    m_model = model;
}

QString AnthropicProvider::model() const
{
    return m_model;
}

bool AnthropicProvider::isConfigured() const
{
    return !m_apiKey.isEmpty();
}

void AnthropicProvider::complete(
    const QList<act::core::LLMMessage> &messages,
    std::function<void(act::core::LLMMessage)> onMessage,
    std::function<void()> onComplete,
    std::function<void(QString, QString)> onError)
{
    if (!isConfigured())
    {
        if (onError)
            onError(QString::fromStdString(act::core::errors::NO_PROVIDER),
                    QStringLiteral("Anthropic provider is not configured. Set an API key."));
        return;
    }

    // Stub: return a placeholder response
    // Real implementation will use INetwork for HTTP POST to Anthropic API
    spdlog::info("AnthropicProvider::complete() called with {} messages (stub)",
                 messages.size());

    m_cancelled = false;

    if (onMessage)
    {
        act::core::LLMMessage response;
        response.role = act::core::MessageRole::Assistant;
        response.content = QStringLiteral("[stub] Anthropic response");
        onMessage(response);
    }

    if (onComplete)
        onComplete();
}

void AnthropicProvider::stream(
    const QList<act::core::LLMMessage> &messages,
    std::function<void(QString)> onToken,
    std::function<void()> onComplete,
    std::function<void(QString, QString)> onError)
{
    if (!isConfigured())
    {
        if (onError)
            onError(QString::fromStdString(act::core::errors::NO_PROVIDER),
                    QStringLiteral("Anthropic provider is not configured. Set an API key."));
        return;
    }

    spdlog::info("AnthropicProvider::stream() called with {} messages (stub)",
                 messages.size());

    m_cancelled = false;

    // Stub: emit a single token then complete
    // Real implementation will use INetwork for SSE connection
    if (onToken)
        onToken(QStringLiteral("[stub]"));

    if (onComplete)
        onComplete();
}

void AnthropicProvider::cancel()
{
    m_cancelled = true;
    spdlog::info("AnthropicProvider: request cancelled");
}

} // namespace act::services
