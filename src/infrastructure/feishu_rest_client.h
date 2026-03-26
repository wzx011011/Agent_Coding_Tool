#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

#include "infrastructure/feishu_protocol.h"
#include "infrastructure/http_network.h"

namespace act::infrastructure
{

/// Feishu REST API client.
/// Reuses HttpNetwork for all HTTP calls. Manages tenant_access_token lifecycle.
class FeishuRestClient : public QObject
{
    Q_OBJECT

public:
    explicit FeishuRestClient(QObject *parent = nullptr);

    void setBaseUrl(const QString &url);
    void setAppCredentials(const QString &appId, const QString &appSecret);
    void setProxy(const QString &host, int port);
    void setTimeoutSeconds(int seconds);

    /// Fetch tenant_access_token. Returns true on success.
    [[nodiscard]] bool fetchToken(QString &tokenOut, QString &errorOut);

    /// Get current cached token. Returns empty if not available.
    [[nodiscard]] QString token() const;

    /// Register for SDK-mode WebSocket, returns the ws URL and service_id.
    [[nodiscard]] bool registerWs(QString &wsUrlOut, QString &errorOut,
                                  int32_t &serviceIdOut);

    /// Send a text message to a chat.
    [[nodiscard]] feishu::FeishuSendResponse sendTextMessage(
        const QString &chatId, const QString &text);

    /// Reply to a specific message with text.
    [[nodiscard]] feishu::FeishuSendResponse replyTextMessage(
        const QString &messageId, const QString &text);

private:
    /// Ensure we have a valid token. Fetches/refreshes as needed.
    [[nodiscard]] bool ensureToken();

    /// Helper: POST JSON to Feishu REST API path.
    [[nodiscard]] bool doPost(
        const QString &path,
        const QJsonObject &body,
        int &statusCodeOut,
        QByteArray &responseBodyOut);

    HttpNetwork m_http;
    QString m_baseUrl = QStringLiteral("https://open.feishu.cn");
    QString m_appId;
    QString m_appSecret;
    QString m_token;
    QDateTime m_tokenExpiry;
    static constexpr int TOKEN_REFRESH_MARGIN_SECONDS = 300;
};

} // namespace act::infrastructure
