#pragma once

#include <QObject>

#include <memory>

#include "core/runtime_event.h"
#include "services/config_manager.h"
#include "services/llm_provider.h"

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

    /// Set tool definitions from ToolRegistry.
    void setToolDefinitions(const QList<QJsonObject> &tools);

    /// Get current provider name.
    [[nodiscard]] QString providerName() const;

signals:
    /// Emitted when a streaming token is received.
    void streamTokenReceived(const QString &token);

private:
    void initProvider();

    ConfigManager &m_config;
    std::unique_ptr<LLMProvider> m_provider;
    QList<QJsonObject> m_toolDefs;
};

} // namespace act::services
