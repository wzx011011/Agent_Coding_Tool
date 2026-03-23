#pragma once

#include <QObject>

#include "services/anthropic_provider.h"
#include "services/config_manager.h"

namespace act::services
{

/// AI engine that manages providers and dispatches chat requests.
class AIEngine : public QObject, public IAIEngine
{
    Q_OBJECT
public:
    explicit AIEngine(ConfigManager &config, QObject *parent = nullptr);

    // IAIEngine
    void chat(const QList<act::core::LLMMessage> &messages,
              std::function<void(act::core::LLMMessage)> onMessage,
              std::function<void()> onComplete,
              std::function<void(QString, QString)> onError) override;
    void cancel() override;
    [[nodiscard]] int estimateTokens(
        const QList<act::core::LLMMessage> &messages) const override;

private:
    ConfigManager &m_config;
    AnthropicProvider m_anthropic;
};

} // namespace act::services
