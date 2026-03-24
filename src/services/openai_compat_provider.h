#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include <memory>

#include <atomic>
#include <functional>

#include "core/types.h"
#include "infrastructure/http_network.h"
#include "services/llm_provider.h"
#include "services/openai_compat_converter.h"

namespace act::services
{

/// OpenAI-compatible API provider (GLM, DeepSeek, etc.).
class OpenAICompatProvider : public LLMProvider
{
public:
    OpenAICompatProvider();
    ~OpenAICompatProvider() override = default;

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
    QString m_baseUrl = QStringLiteral("https://open.bigmodel.cn/api/v1/chat/completions");
    QString m_model;
    std::atomic<bool> m_cancelled{false};
    std::unique_ptr<infrastructure::HttpNetwork> m_network;
    QList<QJsonObject> m_toolDefs;
};

} // namespace act::services
