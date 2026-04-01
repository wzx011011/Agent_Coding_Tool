#include "harness/tools/web_fetch_tool.h"

#include "core/error_codes.h"

#include <spdlog/spdlog.h>

namespace act::harness
{

WebFetchTool::WebFetchTool(act::infrastructure::HttpNetwork &http)
    : m_http(http)
{
}

QString WebFetchTool::name() const
{
    return QStringLiteral("web_fetch");
}

QString WebFetchTool::description() const
{
    return QStringLiteral("Fetch content from a URL via HTTP GET. "
                          "Only http/https schemes and text content are "
                          "supported. Private/internal IPs are blocked. "
                          "Responses larger than 50KB are truncated.");
}

QJsonObject WebFetchTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("url")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("The URL to fetch content from (http/https only)");
        return obj;
    }();

    props[QStringLiteral("headers")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("object");
        obj[QStringLiteral("description")] =
            QStringLiteral("Optional HTTP headers as key-value pairs");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("url")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

bool WebFetchTool::isAllowedScheme(const QString &url)
{
    return url.startsWith(QLatin1String("http://")) ||
           url.startsWith(QLatin1String("https://"));
}

bool WebFetchTool::isPrivateIPv4(quint8 a, quint8 b, quint8 c, quint8 d)
{
    // 127.0.0.0/8 — loopback
    if (a == 127)
        return true;
    // 10.0.0.0/8
    if (a == 10)
        return true;
    // 172.16.0.0/12
    if (a == 172 && b >= 16 && b <= 31)
        return true;
    // 192.168.0.0/16
    if (a == 192 && b == 168)
        return true;
    // 169.254.169.254 — AWS/GCP/Azure cloud metadata
    if (a == 169 && b == 254 && c == 169 && d == 254)
        return true;
    // 0.0.0.0/8 — current network
    if (a == 0)
        return true;
    // 224.0.0.0/4 — multicast
    if (a >= 224 && a <= 239)
        return true;
    return false;
}

act::core::ToolResult WebFetchTool::execute(const QJsonObject &params)
{
    auto urlValue = params.value(QStringLiteral("url"));
    if (!urlValue.isString() || urlValue.toString().trimmed().isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'url' must be a non-empty string"));
    }

    const QString url = urlValue.toString().trimmed();

    // SSRF protection: scheme validation
    if (!isAllowedScheme(url))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("URL scheme must be http or https, got: %1")
                .arg(url.left(10)));
    }

    // Extract host from URL
    QString host = url;
    if (host.startsWith(QLatin1String("https://")))
        host = host.mid(8);
    else if (host.startsWith(QLatin1String("http://")))
        host = host.mid(7);

    int slashPos = host.indexOf(QLatin1Char('/'));
    if (slashPos >= 0)
        host = host.left(slashPos);

    // Strip userinfo (user:pass@host)
    int atPos = host.indexOf(QLatin1Char('@'));
    if (atPos >= 0)
        host = host.mid(atPos + 1);

    // Strip port
    int colonPos = host.indexOf(QLatin1Char(':'));
    if (colonPos >= 0)
        host = host.left(colonPos);

    // SSRF protection: check if host is a private IP address
    // Manual IPv4 parsing to avoid QtNetwork dependency
    QStringList octets = host.split(QLatin1Char('.'));
    if (octets.size() == 4)
    {
        bool allNumeric = true;
        quint8 parts[4] = {};
        for (int i = 0; i < 4; ++i)
        {
            bool ok = false;
            int val = octets[i].toInt(&ok);
            if (!ok || val < 0 || val > 255)
            {
                allNumeric = false;
                break;
            }
            parts[i] = static_cast<quint8>(val);
        }
        if (allNumeric && isPrivateIPv4(parts[0], parts[1], parts[2], parts[3]))
        {
            return act::core::ToolResult::err(
                act::core::errors::INVALID_PARAMS,
                QStringLiteral("URL points to a private/reserved IP address "
                               "(%1). Only public addresses are allowed.")
                    .arg(host));
        }
    }

    // Also reject well-known private hostnames
    if (host == QLatin1String("localhost") ||
        host == QLatin1String("metadata.google.internal"))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("URL host '%1' is a private address. "
                           "Only public hosts are allowed.")
                .arg(host));
    }

    // Build headers map from optional headers parameter
    QMap<QString, QString> headers;
    auto headersValue = params.value(QStringLiteral("headers"));
    if (headersValue.isObject())
    {
        const auto headersObj = headersValue.toObject();
        for (auto it = headersObj.constBegin(); it != headersObj.constEnd();
             ++it)
        {
            headers.insert(it.key(), it.value().toString());
        }
    }

    // Apply shorter timeout for web fetch
    m_http.setTimeoutSeconds(FETCH_TIMEOUT_SECONDS);

    // Perform GET request
    int statusCode = 0;
    QByteArray responseBody;
    bool ok = m_http.httpGet(url, headers, statusCode, responseBody);

    if (!ok)
    {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("HTTP GET request failed for URL: %1").arg(url));
    }

    QJsonObject metadata;
    metadata[QStringLiteral("status_code")] = statusCode;
    metadata[QStringLiteral("truncated")] = false;

    // Binary content detection via null-byte scanning
    bool isBinary = false;
    qsizetype scanLen =
        std::min(responseBody.size(), static_cast<qsizetype>(8192));
    for (qsizetype i = 0; i < scanLen; ++i)
    {
        if (static_cast<unsigned char>(responseBody[i]) == 0)
        {
            isBinary = true;
            break;
        }
    }

    if (isBinary)
    {
        metadata[QStringLiteral("content_type")] = QStringLiteral("binary");
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral(
                "Response has binary content. Only text content is supported."),
            metadata);
    }

    metadata[QStringLiteral("content_type")] = QStringLiteral("text/plain");

    // Truncate if over 50KB
    if (responseBody.size() > MAX_RESPONSE_SIZE)
    {
        responseBody = responseBody.left(MAX_RESPONSE_SIZE);
        metadata[QStringLiteral("truncated")] = true;
    }

    QString output = QString::fromUtf8(responseBody);

    if (statusCode >= 400)
    {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("HTTP GET returned status %1: %2")
                .arg(statusCode)
                .arg(output.left(500)),
            metadata);
    }

    return act::core::ToolResult::ok(output, metadata);
}

act::core::PermissionLevel WebFetchTool::permissionLevel() const
{
    return act::core::PermissionLevel::Network;
}

bool WebFetchTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
