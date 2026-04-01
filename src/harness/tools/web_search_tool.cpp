#include "harness/tools/web_search_tool.h"

#include "core/error_codes.h"

#include <QJsonDocument>
#include <QUrl>
#include <QUrlQuery>

#include <spdlog/spdlog.h>

namespace act::harness
{

WebSearchTool::WebSearchTool(act::infrastructure::HttpNetwork &http)
    : m_http(http)
{
}

QString WebSearchTool::name() const
{
    return QStringLiteral("web_search");
}

QString WebSearchTool::description() const
{
    return QStringLiteral(
        "Search the web using a search API. Returns formatted results "
        "with titles, URLs, and snippets. Requires ACT_SEARCH_API_KEY "
        "and ACT_SEARCH_PROVIDER environment variables to be configured.");
}

QJsonObject WebSearchTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("query")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Search query string");
        return obj;
    }();

    props[QStringLiteral("max_results")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("integer");
        obj[QStringLiteral("description")] =
            QStringLiteral("Maximum number of results (default 10, max 20)");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("query")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult WebSearchTool::execute(const QJsonObject &params)
{
    // 1. Validate query
    auto queryValue = params.value(QStringLiteral("query"));
    if (!queryValue.isString() || queryValue.toString().trimmed().isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'query' must be a non-empty string"));
    }

    const QString query = queryValue.toString().trimmed();

    // 2. Parse max_results
    int maxResults = MAX_RESULTS;
    auto maxResultsValue = params.value(QStringLiteral("max_results"));
    if (maxResultsValue.isDouble())
    {
        maxResults = maxResultsValue.toInt();
        if (maxResults < 1)
            maxResults = 1;
        if (maxResults > 20)
            maxResults = 20;
    }

    // 3. Read environment variables
    const QByteArray apiKey = qgetenv("ACT_SEARCH_API_KEY");
    const QByteArray provider = qgetenv("ACT_SEARCH_PROVIDER");

    if (apiKey.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::AUTH_ERROR,
            QStringLiteral(
                "Web search is not configured. Set ACT_SEARCH_API_KEY "
                "and optionally ACT_SEARCH_PROVIDER (bing or serpapi) "
                "environment variables."));
    }

    // 4. Build search URL based on provider
    QString url;
    QMap<QString, QString> headers;

    const QString providerLower =
        QString::fromUtf8(provider).toLower().trimmed();

    if (providerLower == QLatin1String("serpapi"))
    {
        // SerpAPI: include API key in URL
        QString encodedQuery = QString::fromUtf8(
            QUrl::toPercentEncoding(query));
        url = QStringLiteral(
                  "https://serpapi.com/search.json?q=%1&num=%2&api_key=%3")
                  .arg(encodedQuery)
                  .arg(maxResults)
                  .arg(QString::fromUtf8(apiKey));
    }
    else
    {
        // Default: Bing
        QString encodedQuery = QString::fromUtf8(
            QUrl::toPercentEncoding(query));
        url = QStringLiteral(
                  "https://api.bing.microsoft.com/v7.0/search?q=%1&count=%2")
                  .arg(encodedQuery)
                  .arg(maxResults);
        headers[QStringLiteral("Ocp-Apim-Subscription-Key")] =
            QString::fromUtf8(apiKey);
    }

    // 5. Execute search
    m_http.setTimeoutSeconds(SEARCH_TIMEOUT_SECONDS);

    int statusCode = 0;
    QByteArray responseBody;
    bool ok = m_http.httpGet(url, headers, statusCode, responseBody);

    if (!ok)
    {
        spdlog::warn("WebSearchTool: HTTP GET failed for query '{}'",
                     query.toStdString());
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("Search request failed for query: %1").arg(query));
    }

    if (statusCode == 401 || statusCode == 403)
    {
        return act::core::ToolResult::err(
            act::core::errors::AUTH_ERROR,
            QStringLiteral("Search API authentication failed (HTTP %1). "
                           "Check your ACT_SEARCH_API_KEY.")
                .arg(statusCode));
    }

    if (statusCode == 429)
    {
        return act::core::ToolResult::err(
            act::core::errors::RATE_LIMIT,
            QStringLiteral("Search API rate limit exceeded. "
                           "Please try again later."));
    }

    if (statusCode >= 400)
    {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("Search API returned HTTP %1").arg(statusCode));
    }

    // 6. Parse JSON response and format results
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("Failed to parse search API response: %1")
                .arg(parseError.errorString()));
    }

    QStringList resultLines;
    int resultCount = 0;

    if (providerLower == QLatin1String("serpapi"))
    {
        // SerpAPI: results are in "organic_results" array
        auto organicResults = doc.object()
                                  .value(QStringLiteral("organic_results"))
                                  .toArray();

        for (const auto &item : organicResults)
        {
            if (resultCount >= maxResults)
                break;

            auto obj = item.toObject();
            QString title =
                obj.value(QStringLiteral("title")).toString();
            QString link =
                obj.value(QStringLiteral("link")).toString();
            QString snippet =
                obj.value(QStringLiteral("snippet")).toString();

            resultLines.append(
                QStringLiteral("%1. **[%2](%3)** — %4")
                    .arg(resultCount + 1)
                    .arg(title)
                    .arg(link)
                    .arg(snippet));
            ++resultCount;
        }
    }
    else
    {
        // Bing: results are in "webPages" -> "value" array
        auto webPages = doc.object()
                            .value(QStringLiteral("webPages"))
                            .toObject();
        auto values = webPages.value(QStringLiteral("value")).toArray();

        for (const auto &item : values)
        {
            if (resultCount >= maxResults)
                break;

            auto obj = item.toObject();
            QString title =
                obj.value(QStringLiteral("name")).toString();
            QString link =
                obj.value(QStringLiteral("url")).toString();
            QString snippet =
                obj.value(QStringLiteral("snippet")).toString();

            resultLines.append(
                QStringLiteral("%1. **[%2](%3)** — %4")
                    .arg(resultCount + 1)
                    .arg(title)
                    .arg(link)
                    .arg(snippet));
            ++resultCount;
        }
    }

    if (resultLines.isEmpty())
    {
        return act::core::ToolResult::ok(
            QStringLiteral("No results found for query: %1").arg(query));
    }

    QString output = resultLines.join(QLatin1Char('\n'));

    QJsonObject metadata;
    metadata[QStringLiteral("result_count")] = resultCount;
    metadata[QStringLiteral("query")] = query;

    return act::core::ToolResult::ok(output, metadata);
}

act::core::PermissionLevel WebSearchTool::permissionLevel() const
{
    return act::core::PermissionLevel::Network;
}

bool WebSearchTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
