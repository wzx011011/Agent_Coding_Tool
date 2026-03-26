#pragma once

#include "core/types.h"
#include "infrastructure/http_network.h"
#include "services/llm_provider.h"
#include "services/responses_converter.h"
#include <QJsonObject>
#include <QList>
#include <QString>
#include <atomic>
#include <functional>
#include <memory>

namespace act::services {

class ResponsesProvider : public LLMProvider {
  public:
    ResponsesProvider();
    ~ResponsesProvider() override = default;

    void setApiKey(const QString &key) override;
    void setBaseUrl(const QString &url) override;
    void setModel(const QString &model) override;
    [[nodiscard]] bool isConfigured() const override;
    [[nodiscard]] QString model() const override;

    void complete(const QList<act::core::LLMMessage> &messages, std::function<void(act::core::LLMMessage)> onMessage,
                  std::function<void()> onComplete, std::function<void(QString, QString)> onError) override;

    void stream(const QList<act::core::LLMMessage> &messages, std::function<void(QString)> onToken,
                std::function<void(act::core::LLMMessage)> onMessage, std::function<void()> onComplete,
                std::function<void(QString, QString)> onError) override;

    void cancel() override;
    void setToolDefinitions(const QList<QJsonObject> &tools) override;
    void setProxy(const QString &host, int port) override;

  private:
    [[nodiscard]] act::core::LLMMessage toMessage(const ResponsesConverter::ParsedResponse &response) const;

    QString m_apiKey;
    QString m_baseUrl = QStringLiteral("https://open.bigmodel.cn/api/v1/responses");
    QString m_model;
    std::atomic<bool> m_cancelled{false};
    std::unique_ptr<infrastructure::HttpNetwork> m_network;
    QList<QJsonObject> m_toolDefs;
};

} // namespace act::services