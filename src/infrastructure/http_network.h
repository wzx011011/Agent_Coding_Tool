#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>

#include <functional>

#include "infrastructure/sse_parser.h"

namespace httplib
{
class Client;
}

namespace act::infrastructure
{

/// HTTP client implementation using cpp-httplib.
/// Supports regular POST requests and SSE streaming.
class HttpNetwork
{
public:
    HttpNetwork();
    ~HttpNetwork();

    void setBaseUrl(const QString &url);
    void setProxy(const QString &host, int port);
    void setTimeoutSeconds(int seconds);
    void setDefaultHeaders(const QMap<QString, QString> &headers);

    /// Synchronous HTTP POST request.
    /// Returns true on success, false on network error.
    bool httpRequest(
        const QByteArray &body,
        const QMap<QString, QString> &headers,
        int &statusCode,
        QByteArray &responseBody);

    /// SSE streaming POST request.
    /// onEvent is called for each parsed SSE event.
    /// onComplete is called when the stream ends.
    /// onError is called on failure.
    bool sseRequest(
        const QByteArray &body,
        const QMap<QString, QString> &headers,
        std::function<void(const SseEvent &)> onEvent,
        std::function<void(int, const QByteArray &)> onComplete = {},
        std::function<void(QString, QString)> onError = {});

    void cancel();

private:
    static QString toSchemeHost(const QString &url);
    static QString toPath(const QString &url);

    QString m_baseUrl;
    QString m_proxyHost;
    int m_proxyPort = 0;
    int m_timeoutSeconds = 120;
    QMap<QString, QString> m_defaultHeaders;
    bool m_cancelled = false;
};

} // namespace act::infrastructure
