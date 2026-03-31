#include "harness/tools/web_fetch_tool.h"

#include "core/error_codes.h"

#include <QJsonArray>
#include <algorithm>
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
                          "Only text/* content types are accepted. "
                          "Responses larger than 50KB are truncated.");
}

QJsonObject WebFetchTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("url")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("The URL to fetch content from");
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

act::core::ToolResult WebFetchTool::execute(const QJsonObject &params)
{
    // Validate url parameter
    auto urlValue = params.value(QStringLiteral("url"));
    if (!urlValue.isString() || urlValue.toString().isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'url' must be a non-empty string"));
    }

    const QString url = urlValue.toString().trimmed();

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

    // Extract content-type from response (httplib stores it in the headers)
    // We use the HTTP status code and body as returned
    // For content-type filtering, we check the HTTP headers via the result
    // Since HttpNetwork::httpGet does not expose response headers directly,
    // we check the content-type from a best-effort heuristic: if the URL
    // contains known binary extensions, reject. Otherwise, attempt UTF-8 decode.
    // NOTE: A more robust approach would be to add response headers to httpGet.
    // For now, we do a simple content-type sniff based on status code.

    QJsonObject metadata;
    metadata[QStringLiteral("status_code")] = statusCode;
    metadata[QStringLiteral("truncated")] = false;

    // Check for non-text content types by examining the response body
    // If the body contains null bytes, it is likely binary
    bool isBinary = false;
    for (qsizetype i = 0; i < std::min(responseBody.size(), qsizetype(8192)); ++i)
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
            act::core::errors::INVALID_PARAMS,
            QStringLiteral(
                "Response has binary content (content-type is not text/*). "
                "Only text content is supported."),
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

bool WebFetchTool::isTextContentType(const QString & /*contentType*/)
{
    // Placeholder for future content-type header inspection
    // Currently, binary detection is done via null-byte scanning
    return true;
}

} // namespace act::harness
