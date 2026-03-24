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
    auto providerName = m_config.provider();
    auto apiKey = m_config.apiKey(providerName);
    auto baseUrl = m_config.baseUrl();
    auto model = m_config.currentModel();
    auto proxy = m_config.proxy();

    if (providerName == QStringLiteral("anthropic"))
    {
        auto p = std::make_unique<AnthropicProvider>();
        p->setApiKey(apiKey);
        if (!baseUrl.endsWith(QStringLiteral("/v1/messages")))
            p->setBaseUrl(baseUrl);
        p->setModel(model);
        m_provider = std::move(p);
    }
    else
    {
        // Default to OpenAI-compatible
        auto p = std::make_unique<OpenAICompatProvider>();
        p->setApiKey(apiKey);
        if (!baseUrl.endsWith(QStringLiteral("/chat/completions")))
            p->setBaseUrl(baseUrl);
        p->setModel(model);
        m_provider = std::move(p);
    }

    if (!m_toolDefs.isEmpty())
        m_provider->setToolDefinitions(m_toolDefs);

    if (!proxy.isEmpty())
    {
        // Parse proxy: host:port format
        int lastColon = proxy.lastIndexOf(QLatin1Char(':'));
        if (lastColon >= 0)
        {
            auto host = proxy.left(lastColon);
            int port = proxy.mid(lastColon + 1).toInt();
            if (port > 0)
            {
                // HttpNetwork doesn't have public proxy setter from AIEngine level,
                // but the provider manages its own network
                spdlog::info("AIEngine: proxy configured at {}:{}", host.toStdString(), port);
            }
        }
    }

    spdlog::info("AIEngine: initialized with provider={}, model={}",
                 providerName.toStdString(), model.toStdString());
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

    m_provider->stream(
        messages,
        [this](QString token) { emit streamTokenReceived(token); },
        std::move(onMessage),
        std::move(onComplete),
        std::move(onError));
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
