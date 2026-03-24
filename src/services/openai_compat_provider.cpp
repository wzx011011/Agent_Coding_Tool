#include "services/openai_compat_provider.h"

#include <QJsonDocument>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::services
{

OpenAICompatProvider::OpenAICompatProvider()
    : m_network(std::make_unique<infrastructure::HttpNetwork>())
{
    m_network->setBaseUrl(m_baseUrl + QStringLiteral("/chat/completions"));
}

void OpenAICompatProvider::setApiKey(const QString &key)
{
    m_apiKey = key;
}

void OpenAICompatProvider::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
    if (m_baseUrl.endsWith(QLatin1Char('/')))
        m_baseUrl.chop(1);
    m_network->setBaseUrl(m_baseUrl + QStringLiteral("/chat/completions"));
}

void OpenAICompatProvider::setModel(const QString &model)
{
    m_model = model;
}

QString OpenAICompatProvider::model() const
{
    return m_model;
}

bool OpenAICompatProvider::isConfigured() const
{
    return !m_apiKey.isEmpty();
}

void OpenAICompatProvider::setToolDefinitions(const QList<QJsonObject> &tools)
{
    m_toolDefs = tools;
}

void OpenAICompatProvider::complete(
    const QList<act::core::LLMMessage> &messages,
    std::function<void(act::core::LLMMessage)> onMessage,
    std::function<void()> onComplete,
    std::function<void(QString, QString)> onError)
{
    if (!isConfigured())
    {
        if (onError)
            onError(QString::fromStdString(act::core::errors::NO_PROVIDER),
                    QStringLiteral("OpenAI-compatible provider is not configured. Set an API key."));
        return;
    }

    m_cancelled = false;

    auto request = OpenAICompatConverter::toRequest(messages, m_model, m_toolDefs);
    request[QStringLiteral("stream")] = false;

    QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    auto headers = OpenAICompatConverter::authHeaders(m_apiKey);

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
    auto choices = obj[QStringLiteral("choices")].toArray();
    if (choices.isEmpty())
    {
        if (onError)
            onError(QStringLiteral("EMPTY_RESPONSE"),
                    QStringLiteral("No choices in response"));
        return;
    }

    auto choice = choices[0].toObject();
    auto message = choice[QStringLiteral("message")].toObject();

    act::core::LLMMessage response;
    response.role = act::core::MessageRole::Assistant;
    response.content = message[QStringLiteral("content")].toString();

    auto toolCalls = message[QStringLiteral("tool_calls")].toArray();
    for (const auto &tcVal : toolCalls)
    {
        auto tc = tcVal.toObject();
        auto fn = tc[QStringLiteral("function")].toObject();

        act::core::ToolCall call;
        call.id = tc[QStringLiteral("id")].toString();
        call.name = fn[QStringLiteral("name")].toString();
        call.params = QJsonDocument::fromJson(
            fn[QStringLiteral("arguments")].toString().toUtf8()).object();
        response.toolCalls.append(call);
    }

    if (!response.toolCalls.isEmpty())
        response.toolCall = response.toolCalls.first();

    if (onMessage)
        onMessage(response);
    if (onComplete)
        onComplete();
}

void OpenAICompatProvider::stream(
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
                    QStringLiteral("OpenAI-compatible provider is not configured. Set an API key."));
        return;
    }

    m_cancelled = false;

    auto request = OpenAICompatConverter::toRequest(messages, m_model, m_toolDefs);
    QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    auto headers = OpenAICompatConverter::authHeaders(m_apiKey);

    auto accumulated = std::make_shared<OpenAICompatConverter::ParsedResponse>();

    m_network->sseRequest(
        body,
        headers,
        [this, accumulated, onToken](
            const infrastructure::SseEvent &event)
        {
            if (m_cancelled)
                return;

            // Standard SSE: data is the entire payload
            OpenAICompatConverter::ParsedResponse parsed;
            bool ok = OpenAICompatConverter::parseSseEvent(event.data, parsed);
            if (!ok)
                return; // [DONE]

            // Emit text tokens
            if (!parsed.text.isEmpty() && onToken)
                onToken(parsed.text);

            // Accumulate text
            accumulated->text += parsed.text;

            // Accumulate tool calls
            for (const auto &tc : parsed.toolCalls)
            {
                // find matching or create
                bool found = false;
                for (auto &existing : accumulated->toolCalls)
                {
                    if (existing.name == tc.name &&
                        (existing.id.isEmpty() || existing.id == tc.id))
                    {
                        if (!tc.id.isEmpty())
                            existing.id = tc.id;
                        if (!tc.name.isEmpty())
                            existing.name = tc.name;
                        if (!tc.params.isEmpty())
                            existing.params = tc.params;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    accumulated->toolCalls.append(tc);
            }

            if (!parsed.finishReason.isEmpty())
                accumulated->finishReason = parsed.finishReason;
        },
        [this, accumulated, onMessage, onComplete](int statusCode, const QByteArray &)
        {
            if (m_cancelled)
                return;

            auto finalized = OpenAICompatConverter::finalize(*accumulated);

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

            if (code == QStringLiteral("HTTP_401"))
                code = QString::fromStdString(act::core::errors::AUTH_ERROR);
            else if (code == QStringLiteral("HTTP_429"))
                code = QString::fromStdString(act::core::errors::RATE_LIMIT);

            if (onError)
                onError(code, msg);
        });
}

void OpenAICompatProvider::cancel()
{
    m_cancelled = true;
    if (m_network)
        m_network->cancel();
    spdlog::info("OpenAICompatProvider: request cancelled");
}

void OpenAICompatProvider::setProxy(const QString &host, int port)
{
    if (m_network)
        m_network->setProxy(host, port);
}

} // namespace act::services
