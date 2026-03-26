#include "infrastructure/feishu_rest_client.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QUrl>
#include <QUrlQuery>

#include <spdlog/spdlog.h>

namespace act::infrastructure
{

FeishuRestClient::FeishuRestClient(QObject *parent)
    : QObject(parent)
{
    m_http.setBaseUrl(m_baseUrl);
    m_http.setTimeoutSeconds(30);
}

void FeishuRestClient::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
    m_http.setBaseUrl(url);
}

void FeishuRestClient::setAppCredentials(const QString &appId, const QString &appSecret)
{
    m_appId = appId;
    m_appSecret = appSecret;
}

void FeishuRestClient::setProxy(const QString &host, int port)
{
    m_http.setProxy(host, port);
}

void FeishuRestClient::setTimeoutSeconds(int seconds)
{
    m_http.setTimeoutSeconds(seconds);
}

QString FeishuRestClient::token() const
{
    return m_token;
}

bool FeishuRestClient::fetchToken(QString &tokenOut, QString &errorOut)
{
    QJsonObject body;
    body[QStringLiteral("app_id")] = m_appId;
    body[QStringLiteral("app_secret")] = m_appSecret;

    const auto json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    const QByteArray payload(json);

    QMap<QString, QString> headers;
    headers.insert(QStringLiteral("Content-Type"), QStringLiteral("application/json; charset=utf-8"));

    int statusCode = 0;
    QByteArray responseBody;

    // HttpNetwork splits m_baseUrl into scheme+host and path via toPath/toSchemeHost,
    // so we must include the full API path in the base URL.
    m_http.setBaseUrl(m_baseUrl + QStringLiteral("/open-apis/auth/v3/tenant_access_token/internal"));
    if (!m_http.httpRequest(payload, headers, statusCode, responseBody))
    {
        errorOut = QStringLiteral("Network error fetching token");
        return false;
    }

    if (statusCode != 200)
    {
        errorOut = QStringLiteral("Token API returned HTTP %1").arg(statusCode);
        spdlog::error("FeishuRestClient: token fetch failed: HTTP {}",
                      statusCode);
        return false;
    }

    const auto doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject())
    {
        errorOut = QStringLiteral("Invalid token response JSON");
        return false;
    }

    const auto root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(-1);
    if (code != 0)
    {
        const auto msg = root.value(QStringLiteral("msg")).toString();
        errorOut = QStringLiteral("Token API error: %1").arg(msg);
        spdlog::error("FeishuRestClient: token API error: code={}, msg={}",
                      code, msg.toStdString());
        return false;
    }

    m_token = root.value(QStringLiteral("tenant_access_token")).toString();
    const int expire = root.value(QStringLiteral("expire")).toInt(7200);
    m_tokenExpiry = QDateTime::currentDateTime().addSecs(expire);

    tokenOut = m_token;
    spdlog::info("FeishuRestClient: obtained token, expires in {}s", expire);
    return true;
}

bool FeishuRestClient::ensureToken()
{
    if (m_token.isEmpty())
    {
        QString err;
        return fetchToken(m_token, err);
    }

    if (QDateTime::currentDateTime().secsTo(m_tokenExpiry) < TOKEN_REFRESH_MARGIN_SECONDS)
    {
        spdlog::info("FeishuRestClient: token expiring soon, refreshing");
        QString err;
        QString newToken;
        return fetchToken(newToken, err);
    }

    return true;
}

bool FeishuRestClient::registerWs(QString &wsUrlOut, QString &errorOut,
                                 int32_t &serviceIdOut)
{
    // POST /callback/ws/endpoint with AppID + AppSecret in body
    // (No Bearer token needed — official SDK uses same approach)
    QJsonObject body;
    body[QStringLiteral("AppID")] = m_appId;
    body[QStringLiteral("AppSecret")] = m_appSecret;
    const auto json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    const QByteArray payload(json);

    QMap<QString, QString> headers;
    headers.insert(QStringLiteral("Content-Type"),
                   QStringLiteral("application/json; charset=utf-8"));

    int statusCode = 0;
    QByteArray responseBody;

    m_http.setBaseUrl(m_baseUrl + QStringLiteral("/callback/ws/endpoint"));
    if (!m_http.httpRequest(payload, headers, statusCode, responseBody))
    {
        errorOut = QStringLiteral("Network error registering WebSocket");
        return false;
    }

    if (statusCode != 200)
    {
        errorOut = QStringLiteral("WebSocket register returned HTTP %1").arg(statusCode);
        spdlog::error("FeishuRestClient: ws register failed: HTTP {}",
                      statusCode);
        return false;
    }

    const auto doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject())
    {
        errorOut = QStringLiteral("Invalid ws register response JSON");
        return false;
    }

    const auto root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(-1);
    if (code != 0)
    {
        const auto msg = root.value(QStringLiteral("msg")).toString();
        errorOut = QStringLiteral("WS register error: %1").arg(msg);
        spdlog::error("FeishuRestClient: ws register error: code={}, msg={}",
                      code, msg.toStdString());
        return false;
    }

    // Response: { "code": 0, "data": { "URL": "wss://..." } }
    const auto data = root.value(QStringLiteral("data")).toObject();
    wsUrlOut = data.value(QStringLiteral("URL")).toString();
    if (wsUrlOut.isEmpty())
    {
        errorOut = QStringLiteral("WS register response missing data.URL field");
        spdlog::error("FeishuRestClient: ws register response has no data.URL field");
        return false;
    }

    spdlog::info("FeishuRestClient: obtained ws url: {}",
                 wsUrlOut.toStdString());

    // Extract service_id from URL query params (needed for ping frames)
    serviceIdOut = 0;
    const QUrl wsUrlObj(wsUrlOut);
    const QUrlQuery query(wsUrlObj);
    const auto serviceIdStr = query.queryItemValue(QStringLiteral("service_id"));
    if (!serviceIdStr.isEmpty())
    {
        serviceIdOut = serviceIdStr.toInt();
        spdlog::info("FeishuRestClient: service_id={}", serviceIdOut);
    }

    return true;
}

bool FeishuRestClient::doPost(
    const QString &path,
    const QJsonObject &body,
    int &statusCodeOut,
    QByteArray &responseBodyOut)
{
    if (!ensureToken())
        return false;

    const auto json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    const QByteArray payload(json);

    QMap<QString, QString> headers;
    headers.insert(QStringLiteral("Content-Type"), QStringLiteral("application/json; charset=utf-8"));
    headers.insert(QStringLiteral("Authorization"),
                   QStringLiteral("Bearer ") + m_token);

    // HttpNetwork uses toPath() to extract the path from m_baseUrl, so prepend the API path.
    m_http.setBaseUrl(m_baseUrl + path);
    return m_http.httpRequest(payload, headers, statusCodeOut, responseBodyOut);
}

feishu::FeishuSendResponse FeishuRestClient::sendTextMessage(
    const QString &chatId, const QString &text)
{
    feishu::FeishuSendResponse resp;
    resp.success = false;

    QJsonObject body;
    body[QStringLiteral("receive_id")] = chatId;
    body[QStringLiteral("msg_type")] = QStringLiteral("text");
    body[QStringLiteral("content")] = QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("text"), text}}).toJson(QJsonDocument::Compact));

    int statusCode = 0;
    QByteArray responseBody;

    if (!doPost(QStringLiteral("/open-apis/im/v1/messages?receive_id_type=chat_id"),
                body, statusCode, responseBody))
    {
        resp.code = QStringLiteral("NETWORK_ERROR");
        resp.msg = QStringLiteral("HTTP request failed");
        return resp;
    }

    resp.statusCode = statusCode;

    const auto doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject())
    {
        resp.code = QStringLiteral("INVALID_RESPONSE");
        resp.msg = QStringLiteral("Non-JSON response");
        return resp;
    }

    const auto root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(-1);
    if (code != 0)
    {
        resp.code = QString::number(code);
        resp.msg = root.value(QStringLiteral("msg")).toString();
        spdlog::warn("FeishuRestClient: send message failed: code={}, msg={}",
                     code, resp.msg.toStdString());
        return resp;
    }

    resp.success = true;
    resp.code = QStringLiteral("0");
    resp.messageId = root.value(QStringLiteral("data"))
                         .toObject()
                         .value(QStringLiteral("message_id"))
                         .toString();
    return resp;
}

feishu::FeishuSendResponse FeishuRestClient::replyTextMessage(
    const QString &messageId, const QString &text)
{
    feishu::FeishuSendResponse resp;
    resp.success = false;

    QJsonObject body;
    body[QStringLiteral("msg_type")] = QStringLiteral("text");
    body[QStringLiteral("content")] = QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("text"), text}}).toJson(QJsonDocument::Compact));

    int statusCode = 0;
    QByteArray responseBody;

    const QString path = QStringLiteral("/open-apis/im/v1/messages/%1/reply").arg(messageId);
    if (!doPost(path, body, statusCode, responseBody))
    {
        resp.code = QStringLiteral("NETWORK_ERROR");
        resp.msg = QStringLiteral("HTTP request failed");
        return resp;
    }

    resp.statusCode = statusCode;

    const auto doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject())
    {
        resp.code = QStringLiteral("INVALID_RESPONSE");
        resp.msg = QStringLiteral("Non-JSON response");
        return resp;
    }

    const auto root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(-1);
    if (code != 0)
    {
        resp.code = QString::number(code);
        resp.msg = root.value(QStringLiteral("msg")).toString();
        return resp;
    }

    resp.success = true;
    resp.code = QStringLiteral("0");
    resp.messageId = root.value(QStringLiteral("data"))
                         .toObject()
                         .value(QStringLiteral("message_id"))
                         .toString();
    return resp;
}

} // namespace act::infrastructure
