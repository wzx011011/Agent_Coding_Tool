#include "services/anthropic_provider.h"

#include <QJsonDocument>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::services
{

AnthropicProvider::AnthropicProvider()
    : m_network(std::make_unique<infrastructure::HttpNetwork>())
{
    m_network->setBaseUrl(m_baseUrl + QStringLiteral("/v1/messages"));
}

void AnthropicProvider::setApiKey(const QString &key)
{
    m_apiKey = key;
}

void AnthropicProvider::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
    // If URL already has a path component (e.g., GLM's /api/anthropic),
    // append the default Anthropic path suffix /v1/messages.
    // If it already ends with /v1/messages, use as-is.
    if (m_baseUrl.endsWith(QLatin1Char('/')))
        m_baseUrl.chop(1);
    m_network->setBaseUrl(m_baseUrl + QStringLiteral("/v1/messages"));
}

void AnthropicProvider::setModel(const QString &model)
{
    m_model = model;
}

QString AnthropicProvider::model() const
{
    return m_model;
}

bool AnthropicProvider::isConfigured() const
{
    return !m_apiKey.isEmpty();
}

void AnthropicProvider::setToolDefinitions(const QList<QJsonObject> &tools)
{
    m_toolDefs = tools;
}

void AnthropicProvider::complete(
    const QList<act::core::LLMMessage> &messages,
    std::function<void(act::core::LLMMessage)> onMessage,
    std::function<void()> onComplete,
    std::function<void(QString, QString)> onError)
{
    if (!isConfigured())
    {
        if (onError)
            onError(QString::fromStdString(act::core::errors::NO_PROVIDER),
                    QStringLiteral("Anthropic provider is not configured. Set an API key."));
        return;
    }

    m_cancelled = false;

    auto request = AnthropicConverter::toRequest(messages, m_model, 4096, m_toolDefs);
    // Non-streaming: override stream flag
    request[QStringLiteral("stream")] = false;

    QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    auto headers = AnthropicConverter::authHeaders(m_apiKey);

    int statusCode = 0;
    QByteArray responseBody;

    if (!m_network->httpRequest(body, headers, statusCode, responseBody))
    {
        if (onError)
            onError(QString::fromStdString(act::core::errors::PROVIDER_TIMEOUT),
                    QStringLiteral("HTTP request failed"));
        return;
    }

    if (statusCode == 401)
    {
        if (onError)
            onError(QString::fromStdString(act::core::errors::AUTH_ERROR),
                    QStringLiteral("Authentication failed (401)"));
        return;
    }
    if (statusCode == 429)
    {
        if (onError)
            onError(QString::fromStdString(act::core::errors::RATE_LIMIT),
                    QStringLiteral("Rate limited (429)"));
        return;
    }
    if (statusCode < 200 || statusCode >= 300)
    {
        if (onError)
            onError(QStringLiteral("HTTP_%1").arg(statusCode),
                    QString::fromUtf8(responseBody));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject())
    {
        if (onError)
            onError(QStringLiteral("PARSE_ERROR"),
                    QStringLiteral("Invalid JSON response"));
        return;
    }

    auto obj = doc.object();
    act::core::LLMMessage response;
    response.role = act::core::MessageRole::Assistant;

    auto content = obj[QStringLiteral("content")].toArray();
    for (const auto &block : content)
    {
        auto blockObj = block.toObject();
        auto type = blockObj[QStringLiteral("type")].toString();

        if (type == QLatin1String("text"))
        {
            response.content += blockObj[QStringLiteral("text")].toString();
        }
        else if (type == QLatin1String("tool_use"))
        {
            act::core::ToolCall tc;
            tc.id = blockObj[QStringLiteral("id")].toString();
            tc.name = blockObj[QStringLiteral("name")].toString();
            tc.params = blockObj[QStringLiteral("input")].toObject();
            response.toolCalls.append(tc);
        }
    }

    // Backward compat: first tool call
    if (!response.toolCalls.isEmpty())
        response.toolCall = response.toolCalls.first();

    if (onMessage)
        onMessage(response);
    if (onComplete)
        onComplete();
}

void AnthropicProvider::stream(
    const QList<act::core::LLMMessage> &messages,
    std::function<void(QString)> onToken,
    std::function<void(act::core::LLMMessage)> onMessage,
    std::function<void()> onComplete,
    std::function<void(QString, QString)> onError)
{
    if (!isConfigured())
    {
        if (onError)
            onError(QString::fromStdString(act::core::errors::NO_PROVIDER),
                    QStringLiteral("Anthropic provider is not configured. Set an API key."));
        return;
    }

    m_cancelled = false;

    auto request = AnthropicConverter::toRequest(messages, m_model, 4096, m_toolDefs);
    QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    auto headers = AnthropicConverter::authHeaders(m_apiKey);

    auto accumulated = std::make_shared<AnthropicConverter::ParsedResponse>();

    m_network->sseRequest(
        body,
        headers,
        [this, accumulated, onToken, onMessage](
            const infrastructure::SseEvent &event)
        {
            if (m_cancelled)
                return;

            // Check for [DONE]
            if (event.data.trimmed() == QStringLiteral("[DONE]"))
                return;

            QJsonDocument doc = QJsonDocument::fromJson(event.data.toUtf8());
            if (!doc.isObject())
                return;

            auto parsed = AnthropicConverter::parseSseEvent(
                event.eventType, doc.object());

            // Emit text tokens
            if (!parsed.text.isEmpty() && onToken)
            {
                // Check if this is a tool input delta (not user-visible text)
                bool isToolInput = !parsed.toolCalls.isEmpty() &&
                    event.eventType == QLatin1String("content_block_delta");
                if (!isToolInput)
                    onToken(parsed.text);
            }

            // Accumulate text
            if (!parsed.text.isEmpty())
            {
                auto contentBlockType = event.eventType;
                // Don't mix tool input JSON into the text response
                if (contentBlockType != QLatin1String("content_block_delta") ||
                    doc.object()[QStringLiteral("delta")].toObject()
                        [QStringLiteral("type")].toString() == QLatin1String("text_delta"))
                {
                    accumulated->text += parsed.text;
                }
            }

            // Accumulate tool calls
            for (const auto &tc : parsed.toolCalls)
            {
                if (tc.id.isEmpty() && tc.name.isEmpty())
                    continue;
                // If this is a new tool call with an ID, add it
                if (!tc.id.isEmpty())
                {
                    bool found = false;
                    for (auto &existing : accumulated->toolCalls)
                    {
                        if (existing.id == tc.id)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        accumulated->toolCalls.append(tc);
                }
                // Accumulate input_json_delta into the last tool call
                if (!tc.params.isEmpty() && !accumulated->toolCalls.isEmpty())
                {
                    auto &last = accumulated->toolCalls.last();
                    // Parse partial JSON and merge
                    auto partial = QJsonDocument::fromJson(
                        tc.params[QStringLiteral("_partial")].toString().toUtf8());
                    if (partial.isObject())
                        last.params = partial.object();
                }
            }

            if (!parsed.stopReason.isEmpty())
                accumulated->stopReason = parsed.stopReason;
        },
        [this, accumulated, onMessage, onComplete](int statusCode, const QByteArray &)
        {
            if (m_cancelled)
                return;

            auto finalized = AnthropicConverter::finalize(*accumulated);

            act::core::LLMMessage response;
            response.role = act::core::MessageRole::Assistant;
            response.content = finalized.text;

            for (const auto &tc : finalized.toolCalls)
                response.toolCalls.append(tc);
            if (!response.toolCalls.isEmpty())
                response.toolCall = response.toolCalls.first();

            if (onMessage)
                onMessage(response);
            if (onComplete)
                onComplete();
        },
        [this, onError](QString code, QString msg)
        {
            if (m_cancelled)
                return;

            // Map HTTP status codes to error codes
            if (code == QStringLiteral("HTTP_401"))
                code = QString::fromStdString(act::core::errors::AUTH_ERROR);
            else if (code == QStringLiteral("HTTP_429"))
                code = QString::fromStdString(act::core::errors::RATE_LIMIT);

            if (onError)
                onError(code, msg);
        });
}

void AnthropicProvider::cancel()
{
    m_cancelled = true;
    if (m_network)
        m_network->cancel();
    spdlog::info("AnthropicProvider: request cancelled");
}

} // namespace act::services
