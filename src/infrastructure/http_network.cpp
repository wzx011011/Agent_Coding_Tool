#include "infrastructure/http_network.h"

#include <sstream>

#include <QCoreApplication>

#include <httplib.h>

#include <spdlog/spdlog.h>

namespace act::infrastructure
{

HttpNetwork::HttpNetwork() = default;

HttpNetwork::~HttpNetwork() = default;

void HttpNetwork::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
}

void HttpNetwork::setProxy(const QString &host, int port)
{
    m_proxyHost = host;
    m_proxyPort = port;
}

void HttpNetwork::setTimeoutSeconds(int seconds)
{
    m_timeoutSeconds = seconds;
}

void HttpNetwork::setDefaultHeaders(const QMap<QString, QString> &headers)
{
    m_defaultHeaders = headers;
}

QString HttpNetwork::toSchemeHost(const QString &url)
{
    // Extract scheme+host from URL, e.g., "https://api.anthropic.com"
    QString u = url;
    if (u.startsWith(QStringLiteral("https://")))
        u = u.mid(8);
    else if (u.startsWith(QStringLiteral("http://")))
        u = u.mid(7);
    else
        return url;

    int slashPos = u.indexOf(QLatin1Char('/'));
    if (slashPos >= 0)
        u = u.left(slashPos);

    return url.left(url.indexOf(u) + u.length());
}

QString HttpNetwork::toPath(const QString &url)
{
    QString u = url;
    if (u.startsWith(QStringLiteral("https://")))
        u = u.mid(8);
    else if (u.startsWith(QStringLiteral("http://")))
        u = u.mid(7);

    int slashPos = u.indexOf(QLatin1Char('/'));
    if (slashPos >= 0)
        return u.mid(slashPos);
    return QStringLiteral("/");
}

bool HttpNetwork::httpRequest(
    const QByteArray &body,
    const QMap<QString, QString> &headers,
    int &statusCode,
    QByteArray &responseBody)
{
    m_cancelled = false;

    QString schemeHost = toSchemeHost(m_baseUrl);
    QString path = toPath(m_baseUrl);

    httplib::Client client(schemeHost.toStdString());
    client.set_connection_timeout(m_timeoutSeconds);
    client.set_read_timeout(m_timeoutSeconds);
    client.set_write_timeout(m_timeoutSeconds);

#ifdef CPPHTTPLIB_SSL_ENABLED
    client.enable_server_certificate_verification(false);
#endif

    if (!m_proxyHost.isEmpty() && m_proxyPort > 0)
        client.set_proxy(m_proxyHost.toStdString(), m_proxyPort);

    httplib::Headers reqHeaders;
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        reqHeaders.insert({it.key().toStdString(), it.value().toStdString()});
    for (auto it = m_defaultHeaders.constBegin(); it != m_defaultHeaders.constEnd(); ++it)
    {
        if (!reqHeaders.contains(it.key().toStdString()))
            reqHeaders.insert({it.key().toStdString(), it.value().toStdString()});
    }

    auto result = client.Post(
        path.toStdString(),
        reqHeaders,
        body.toStdString(),
        "application/json");

    if (!result)
    {
        spdlog::error("HttpNetwork: request failed: {}",
                      static_cast<int>(result.error()));
        return false;
    }

    statusCode = result->status;
    responseBody = QByteArray(result->body.data(),
                              static_cast<int>(result->body.size()));
    return true;
}

bool HttpNetwork::sseRequest(
    const QByteArray &body,
    const QMap<QString, QString> &headers,
    std::function<void(const SseEvent &)> onEvent,
    std::function<void(int, const QByteArray &)> onComplete,
    std::function<void(QString, QString)> onError)
{
    m_cancelled = false;

    QString schemeHost = toSchemeHost(m_baseUrl);
    QString path = toPath(m_baseUrl);

    httplib::Client client(schemeHost.toStdString());
    client.set_connection_timeout(m_timeoutSeconds);
    client.set_read_timeout(m_timeoutSeconds);
    client.set_write_timeout(m_timeoutSeconds);

#ifdef CPPHTTPLIB_SSL_ENABLED
    client.enable_server_certificate_verification(false);
#endif

    if (!m_proxyHost.isEmpty() && m_proxyPort > 0)
        client.set_proxy(m_proxyHost.toStdString(), m_proxyPort);

    httplib::Headers reqHeaders;
    reqHeaders.insert({"Accept", "text/event-stream"});
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        reqHeaders.insert({it.key().toStdString(), it.value().toStdString()});
    for (auto it = m_defaultHeaders.constBegin(); it != m_defaultHeaders.constEnd(); ++it)
    {
        if (!reqHeaders.contains(it.key().toStdString()))
            reqHeaders.insert({it.key().toStdString(), it.value().toStdString()});
    }

    SseParser parser;

    auto result = client.Post(
        path.toStdString(),
        reqHeaders,
        body.toStdString(),
        "application/json",
        [&](const char *data, size_t len) -> bool
        {
            if (m_cancelled)
                return false;

            QByteArray chunk(data, static_cast<int>(len));
            auto events = parser.feed(chunk);
            for (const auto &event : events)
            {
                if (onEvent)
                    onEvent(event);
            }
            // Pump the Qt event loop so queued signals (e.g.
            // streamTokenReceived) and timers (e.g. thinking spinner)
            // get a chance to run during the synchronous HTTP read.
            // Guard against reentrancy: a slot triggered here must not
            // re-enter the SSE callback (e.g. by cancelling the request).
            if (!m_inSseCallback) {
                m_inSseCallback = true;
                QCoreApplication::processEvents(QEventLoop::AllEvents, 0);
                m_inSseCallback = false;
            }
            return true;
        });

    if (!result)
    {
        if (onError)
            onError(QStringLiteral("NETWORK_ERROR"),
                    QStringLiteral("SSE connection failed: %1")
                        .arg(static_cast<int>(result.error())));
        return false;
    }

    // Check status code from result
    if (result->status < 200 || result->status >= 300)
    {
        if (onError)
            onError(QStringLiteral("HTTP_%1").arg(result->status),
                    QString::fromStdString(result->body));
        return false;
    }

    // Flush any remaining incomplete event
    auto remaining = parser.flush();
    for (const auto &event : remaining)
    {
        if (onEvent)
            onEvent(event);
    }

    if (onComplete)
        onComplete(result->status, QByteArray(result->body.data(),
                                                    static_cast<int>(result->body.size())));

    return true;
}

void HttpNetwork::cancel()
{
    m_cancelled = true;
    spdlog::info("HttpNetwork: cancel requested");
}

} // namespace act::infrastructure
