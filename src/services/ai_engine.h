#pragma once

#include <QObject>

#include <memory>

#include "core/runtime_event.h"
#include "services/config_manager.h"
#include "services/llm_provider.h"

namespace act::services
{

/// AI engine that manages providers and dispatches chat requests.
/// Supports fallback: when the primary provider fails with retryable
/// errors (401/429/timeout), automatically tries fallback providers.
class AIEngine : public QObject, public IAIEngine
{
    Q_OBJECT
public:
    explicit AIEngine(ConfigManager &config, QObject *parent = nullptr);

    [[nodiscard]] std::unique_ptr<AIEngine> createDetachedInstance() const;

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

    /// Reinitialize the provider from current config (call after config changes).
    /// Returns true if the provider was successfully created.
    [[nodiscard]] bool reinitializeProvider();

    /// Check if an error code is retryable (should trigger fallback).
    [[nodiscard]] static bool isRetryableError(const QString &errorCode);

signals:
    /// Emitted when a streaming token is received.
    void streamTokenReceived(const QString &token);

    /// Emitted when fallback to another provider occurs.
    void fallbackTriggered(const QString &fromProvider,
                            const QString &toProvider,
                            const QString &reason);

private:
    void initProvider();

    /// Create a provider by name from config settings.
    [[nodiscard]] std::unique_ptr<LLMProvider> createProvider(
        const QString &providerName) const;

    /// Try streaming with the next provider in the fallback chain.
    void tryStreamWithProvider(
        const QList<act::core::LLMMessage> &messages,
        const QStringList &providerNames,
        std::function<void(act::core::LLMMessage)> onMessage,
        std::function<void()> onComplete,
        std::function<void(QString, QString)> onError,
        int currentIndex);

    ConfigManager &m_config;
    std::unique_ptr<LLMProvider> m_provider;
    QList<QJsonObject> m_toolDefs;
};

} // namespace act::services
