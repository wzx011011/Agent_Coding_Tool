#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>

#include "infrastructure/sse_parser.h"

namespace httplib
{
class Client;
}

namespace act::infrastructure
{

/// Default per-request timeout in seconds.
inline constexpr int kDefaultTimeoutSeconds = 120;

/// HTTP client implementation using cpp-httplib.
/// Supports regular POST/GET requests and SSE streaming.
class HttpNetwork
{
public:
    HttpNetwork();
    ~HttpNetwork();

    void setBaseUrl(const QString &url);
    void setProxy(const QString &host, int port);
    void setTimeoutSeconds(int seconds);
    void setDefaultHeaders(const QMap<QString, QString> &headers);

    /// Control whether SSL server certificate verification is enabled.
    /// Default: true. Set to false only for development/testing.
    void setSslVerificationEnabled(bool enabled);

    [[nodiscard]] bool sslVerificationEnabled() const;

    /// Synchronous HTTP POST request.
    /// Returns true on success, false on network error.
    [[nodiscard]] bool httpRequest(
        const QByteArray &body,
        const QMap<QString, QString> &headers,
        int &statusCode,
        QByteArray &responseBody);

    /// Synchronous HTTP GET request.
    /// Returns true on success, false on network error.
    [[nodiscard]] bool httpGet(
        const QString &url,
        const QMap<QString, QString> &headers,
        int &statusCode,
        QByteArray &responseBody);

    /// SSE streaming POST request.
    /// onEvent is called for each parsed SSE event.
    /// onComplete is called when the stream ends.
    /// onError is called on failure.
    [[nodiscard]] bool sseRequest(
        const QByteArray &body,
        const QMap<QString, QString> &headers,
        std::function<void(const SseEvent &)> onEvent,
        std::function<void(int, const QByteArray &)> onComplete = {},
        std::function<void(QString, QString)> onError = {});

    void cancel();

private:
    /// Create and configure an httplib::Client for the given scheme+host.
    [[nodiscard]] std::unique_ptr<httplib::Client> createClient(
        const QString &schemeHost) const;

    /// Merge request-specific headers with default headers.
    /// Request-specific headers take precedence.
    /// Returns merged headers as a QMap (used to construct httplib::Headers
    /// internally in the .cpp).
    [[nodiscard]] QMap<QString, QString> mergeHeadersMap(
        const QMap<QString, QString> &requestHeaders) const;

    /// Convert QMap to httplib::Headers and apply to client.
    static void applyHeaders(
        httplib::Client &client,
        const QMap<QString, QString> &headersMap);

    static QString toSchemeHost(const QString &url);
    static QString toPath(const QString &url);

    QString m_baseUrl;
    QString m_proxyHost;
    int m_proxyPort = 0;
    int m_timeoutSeconds = kDefaultTimeoutSeconds;
    QMap<QString, QString> m_defaultHeaders;
    std::atomic<bool> m_cancelled{false};
    std::atomic<bool> m_inSseCallback{false};
    bool m_sslVerificationEnabled = true;
};

} // namespace act::infrastructure
