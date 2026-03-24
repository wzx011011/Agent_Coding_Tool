#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include <memory>

#include <functional>

#include "core/types.h"
#include "infrastructure/http_network.h"
#include "services/anthropic_converter.h"
#include "services/llm_provider.h"

namespace act::services
{

/// Anthropic Messages API provider with real HTTP/SSE implementation.
class AnthropicProvider : public LLMProvider
{
public:
    AnthropicProvider();
    ~AnthropicProvider() override = default;

    // LLMProvider
    void setApiKey(const QString &key) override;
    void setBaseUrl(const QString &url) override;
    void setModel(const QString &model) override;
    [[nodiscard]] bool isConfigured() const override;
    [[nodiscard]] QString model() const override;

    void complete(
        const QList<act::core::LLMMessage> &messages,
        std::function<void(act::core::LLMMessage)> onMessage,
        std::function<void()> onComplete,
        std::function<void(QString, QString)> onError) override;

    void stream(
        const QList<act::core::LLMMessage> &messages,
        std::function<void(QString)> onToken,
        std::function<void(act::core::LLMMessage)> onMessage,
        std::function<void()> onComplete,
        std::function<void(QString, QString)> onError) override;

    void cancel() override;
    void setToolDefinitions(const QList<QJsonObject> &tools) override;
    void setProxy(const QString &host, int port) override;

private:
    QString m_apiKey;
    QString m_baseUrl = QStringLiteral("https://api.anthropic.com");
    QString m_model;
    bool m_cancelled = false;
    std::unique_ptr<infrastructure::HttpNetwork> m_network;
    QList<QJsonObject> m_toolDefs;
};

} // namespace act::services
