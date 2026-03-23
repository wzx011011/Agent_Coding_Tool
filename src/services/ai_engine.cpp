#include "services/ai_engine.h"

#include <spdlog/spdlog.h>

#include "services/anthropic_provider.h"
#include "services/config_manager.h"

namespace act::services
{

AIEngine::AIEngine(ConfigManager &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_anthropic.setApiKey(m_config.apiKey(QStringLiteral("anthropic")));
    m_anthropic.setModel(m_config.currentModel());
}

void AIEngine::chat(
    const QList<act::core::LLMMessage> &messages,
    std::function<void(act::core::LLMMessage)> onMessage,
    std::function<void()> onComplete,
    std::function<void(QString, QString)> onError)
{
    m_anthropic.complete(messages, std::move(onMessage),
                        std::move(onComplete), std::move(onError));
}

void AIEngine::cancel()
{
    m_anthropic.cancel();
}

int AIEngine::estimateTokens(
    const QList<act::core::LLMMessage> &messages) const
{
    // P1a baseline: chars / 3.5 heuristic
    long long totalChars = 0;
    for (const auto &msg : messages)
    {
        totalChars += msg.content.length();
        totalChars += msg.toolCall.name.length();
        totalChars += msg.toolCall.id.length();
        // Rough JSON overhead for tool call params
        totalChars += 50;
    }
    return static_cast<int>(totalChars / 3.5);
}

} // namespace act::services
