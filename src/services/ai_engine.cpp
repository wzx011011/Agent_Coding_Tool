#include "services/ai_engine.h"

#include <spdlog/spdlog.h>

#include "core/error_codes.h"
#include "services/anthropic_provider.h"
#include "services/config_manager.h"
#include "services/openai_compat_provider.h"

namespace act::services
{

AIEngine::AIEngine(ConfigManager &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    initProvider();
}

void AIEngine::initProvider()
{
    m_provider = createProvider(m_config.provider());

    spdlog::info("AIEngine: initialized with provider={}, model={}",
                 m_config.provider().toStdString(),
                 m_config.currentModel().toStdString());
}

std::unique_ptr<LLMProvider> AIEngine::createProvider(
    const QString &providerName) const
{
    auto apiKey = m_config.apiKey(providerName);
    auto baseUrl = m_config.defaultBaseUrl(providerName);
    auto model = m_config.currentModel();
    auto proxy = m_config.proxy();

    std::unique_ptr<LLMProvider> provider;

    if (providerName == QStringLiteral("anthropic"))
        provider = std::make_unique<AnthropicProvider>();
    else
        provider = std::make_unique<OpenAICompatProvider>();

    provider->setApiKey(apiKey);
    provider->setBaseUrl(baseUrl);
    provider->setModel(model);

    if (!m_toolDefs.isEmpty())
        provider->setToolDefinitions(m_toolDefs);

    if (!proxy.isEmpty())
    {
        int lastColon = proxy.lastIndexOf(QLatin1Char(':'));
        if (lastColon >= 0)
        {
            auto host = proxy.left(lastColon);
            int port = proxy.mid(lastColon + 1).toInt();
            if (port > 0)
                provider->setProxy(host, port);
        }
    }

    return provider;
}

bool AIEngine::isRetryableError(const QString &errorCode)
{
    return errorCode == QLatin1String(act::core::errors::AUTH_ERROR) ||
           errorCode == QLatin1String(act::core::errors::RATE_LIMIT) ||
           errorCode == QLatin1String(act::core::errors::PROVIDER_TIMEOUT);
}

void AIEngine::chat(
    const QList<act::core::LLMMessage> &messages,
    std::function<void(act::core::LLMMessage)> onMessage,
    std::function<void()> onComplete,
    std::function<void(QString, QString)> onError)
{
    if (!m_provider)
    {
        if (onError)
            onError(QString::fromStdString(act::core::errors::NO_PROVIDER),
                    QStringLiteral("No AI provider configured"));
        return;
    }

    QStringList providerNames;
    providerNames.append(m_config.provider());
    providerNames.append(m_config.fallbackProviders());

    tryStreamWithProvider(messages, providerNames,
                         std::move(onMessage), std::move(onComplete),
                         std::move(onError), 0);
}

void AIEngine::tryStreamWithProvider(
    const QList<act::core::LLMMessage> &messages,
    const QStringList &providerNames,
    std::function<void(act::core::LLMMessage)> onMessage,
    std::function<void()> onComplete,
    std::function<void(QString, QString)> onError,
    int currentIndex)
{
    if (currentIndex >= providerNames.size())
    {
        if (onError)
            onError(QString::fromStdString(act::core::errors::NO_PROVIDER),
                    QStringLiteral("All providers exhausted"));
        return;
    }

    const auto &name = providerNames[currentIndex];

    if (currentIndex == 0)
    {
        // Primary provider is already in m_provider
    }
    else
    {
        m_provider = createProvider(name);
        emit fallbackTriggered(providerNames[0], name,
                              QStringLiteral("retryable error"));
        spdlog::warn("AIEngine: falling back to provider {}",
                     name.toStdString());
    }

    if (!m_provider)
    {
        tryStreamWithProvider(messages, providerNames,
                             std::move(onMessage), std::move(onComplete),
                             std::move(onError), currentIndex + 1);
        return;
    }

    // Capture by value for the error handler so it outlives the stream call
    auto providerNamesCopy = providerNames;
    auto nextIdx = currentIndex + 1;

    m_provider->stream(
        messages,
        [this](QString token) { emit streamTokenReceived(token); },

        std::move(onMessage),

        std::move(onComplete),

        [this, messages, providerNamesCopy, nextIdx,
         onError](QString errCode, QString errMsg) mutable
        {
            if (isRetryableError(errCode) &&
                nextIdx < providerNamesCopy.size())
            {
                spdlog::warn("AIEngine: provider {} failed ({}), "
                             "trying fallback",
                             providerNamesCopy[nextIdx - 1].toStdString(),
                             errCode.toStdString());
                tryStreamWithProvider(messages, providerNamesCopy,
                                     nullptr, nullptr,
                                     std::move(onError), nextIdx);
            }
            else
            {
                if (onError)
                    onError(std::move(errCode), std::move(errMsg));
            }
        });
}

void AIEngine::cancel()
{
    if (m_provider)
        m_provider->cancel();
}

int AIEngine::estimateTokens(
    const QList<act::core::LLMMessage> &messages) const
{
    long long totalChars = 0;
    for (const auto &msg : messages)
    {
        totalChars += msg.content.length();
        totalChars += msg.toolCall.name.length();
        totalChars += msg.toolCall.id.length();
        totalChars += 50; // Rough JSON overhead for tool call params
    }
    return static_cast<int>(totalChars / 3.5);
}

void AIEngine::setToolDefinitions(const QList<QJsonObject> &tools)
{
    m_toolDefs = tools;
    if (m_provider)
        m_provider->setToolDefinitions(tools);
}

QString AIEngine::providerName() const
{
    return m_config.provider();
}

} // namespace act::services
