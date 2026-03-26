#include "services/responses_provider.h"

#include "core/error_codes.h"
#include <QJsonDocument>
#include <spdlog/spdlog.h>

namespace act::services {

namespace {

void mergeToolCall(QList<act::core::ToolCall> &toolCalls, const act::core::ToolCall &call) {
    if (call.id.isEmpty() && call.name.isEmpty() && call.params.isEmpty())
        return;

    for (auto &existing : toolCalls) {
        if ((!call.id.isEmpty() && existing.id == call.id) ||
            (call.id.isEmpty() && !call.name.isEmpty() && existing.name == call.name)) {
            if (!call.id.isEmpty())
                existing.id = call.id;
            if (!call.name.isEmpty())
                existing.name = call.name;
            if (!call.params.isEmpty())
                existing.params = call.params;
            return;
        }
    }

    toolCalls.append(call);
}

} // namespace

ResponsesProvider::ResponsesProvider() : m_network(std::make_unique<infrastructure::HttpNetwork>()) {
    m_network->setBaseUrl(m_baseUrl);
}

void ResponsesProvider::setApiKey(const QString &key) {
    m_apiKey = key;
}

void ResponsesProvider::setBaseUrl(const QString &url) {
    m_baseUrl = url;
    if (m_baseUrl.endsWith(QLatin1Char('/')))
        m_baseUrl.chop(1);

    static const QLatin1String kPathSuffix("/responses");
    if (!m_baseUrl.endsWith(kPathSuffix))
        m_baseUrl += kPathSuffix;

    m_network->setBaseUrl(m_baseUrl);
}

void ResponsesProvider::setModel(const QString &model) {
    m_model = model;
}

bool ResponsesProvider::isConfigured() const {
    return !m_apiKey.isEmpty();
}

QString ResponsesProvider::model() const {
    return m_model;
}

void ResponsesProvider::complete(const QList<act::core::LLMMessage> &messages,
                                 std::function<void(act::core::LLMMessage)> onMessage, std::function<void()> onComplete,
                                 std::function<void(QString, QString)> onError) {
    if (!isConfigured()) {
        if (onError)
            onError(QString::fromStdString(act::core::errors::NO_PROVIDER),
                    QStringLiteral("Responses provider is not configured. Set an API key."));
        return;
    }

    m_cancelled = false;

    auto request = ResponsesConverter::toRequest(messages, m_model, m_toolDefs);
    request[QStringLiteral("stream")] = false;

    const auto body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    const auto headers = ResponsesConverter::authHeaders(m_apiKey);

    int statusCode = 0;
    QByteArray responseBody;
    if (!m_network->httpRequest(body, headers, statusCode, responseBody)) {
        if (onError)
            onError(QString::fromStdString(act::core::errors::PROVIDER_TIMEOUT), QStringLiteral("HTTP request failed"));
        return;
    }

    if (statusCode == 401) {
        if (onError)
            onError(QString::fromStdString(act::core::errors::AUTH_ERROR),
                    QStringLiteral("Authentication failed (401)"));
        return;
    }
    if (statusCode == 429) {
        if (onError)
            onError(QString::fromStdString(act::core::errors::RATE_LIMIT), QStringLiteral("Rate limited (429)"));
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (onError)
            onError(QStringLiteral("HTTP_%1").arg(statusCode), QString::fromUtf8(responseBody));
        return;
    }

    const auto parsed = ResponsesConverter::parseResponseBody(responseBody);
    if (!parsed.errorCode.isEmpty()) {
        if (onError)
            onError(parsed.errorCode, parsed.errorMessage);
        return;
    }

    if (onMessage)
        onMessage(toMessage(parsed));
    if (onComplete)
        onComplete();
}

void ResponsesProvider::stream(const QList<act::core::LLMMessage> &messages, std::function<void(QString)> onToken,
                               std::function<void(act::core::LLMMessage)> onMessage, std::function<void()> onComplete,
                               std::function<void(QString, QString)> onError) {
    if (!isConfigured()) {
        if (onError)
            onError(QString::fromStdString(act::core::errors::NO_PROVIDER),
                    QStringLiteral("Responses provider is not configured. Set an API key."));
        return;
    }

    m_cancelled = false;

    const auto request = ResponsesConverter::toRequest(messages, m_model, m_toolDefs);
    const auto body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    const auto headers = ResponsesConverter::authHeaders(m_apiKey);

    auto accumulated = std::make_shared<ResponsesConverter::ParsedResponse>();
    auto sawStreamPayload = std::make_shared<bool>(false);
    auto sawCompletion = std::make_shared<bool>(false);

    m_network->sseRequest(
        body, headers,
        [this, accumulated, sawStreamPayload, onToken](const infrastructure::SseEvent &event) {
            if (m_cancelled)
                return;

            ResponsesConverter::ParsedResponse parsed;
            const bool shouldContinue = ResponsesConverter::parseSseEvent(event, parsed);
            if (!shouldContinue)
                return;

            *sawStreamPayload = true;

            if (!parsed.text.isEmpty()) {
                accumulated->text += parsed.text;
                if (onToken)
                    onToken(parsed.text);
            }

            for (const auto &call : parsed.toolCalls)
                mergeToolCall(accumulated->toolCalls, call);

            if (!parsed.status.isEmpty())
                accumulated->status = parsed.status;
            if (!parsed.errorCode.isEmpty())
                accumulated->errorCode = parsed.errorCode;
            if (!parsed.errorMessage.isEmpty())
                accumulated->errorMessage = parsed.errorMessage;
        },
        [this, accumulated, sawStreamPayload, sawCompletion, onMessage, onComplete,
         onError](int, const QByteArray &responseBody) {
            if (m_cancelled)
                return;

            if (!*sawStreamPayload && !responseBody.isEmpty()) {
                const auto parsedBody = ResponsesConverter::parseResponseBody(responseBody);
                if (parsedBody.errorCode.isEmpty()) {
                    accumulated->text = parsedBody.text;
                    accumulated->toolCalls = parsedBody.toolCalls;
                    accumulated->status = parsedBody.status;
                    accumulated->errorCode = parsedBody.errorCode;
                    accumulated->errorMessage = parsedBody.errorMessage;
                }
            }

            const auto finalized = ResponsesConverter::finalize(*accumulated);
            if (!finalized.errorCode.isEmpty()) {
                if (onError)
                    onError(finalized.errorCode, finalized.errorMessage);
                return;
            }

            *sawCompletion = true;
            if (onMessage)
                onMessage(toMessage(finalized));
            if (onComplete)
                onComplete();
        },
        [this, messages, sawStreamPayload, sawCompletion, onMessage, onComplete, onError](QString code, QString msg) {
            if (m_cancelled)
                return;

            if (!*sawStreamPayload && !*sawCompletion &&
                (code == QStringLiteral("NETWORK_ERROR") || code.startsWith(QStringLiteral("HTTP_5")))) {
                complete(messages, onMessage, onComplete, onError);
                return;
            }

            if (code == QStringLiteral("HTTP_401"))
                code = QString::fromStdString(act::core::errors::AUTH_ERROR);
            else if (code == QStringLiteral("HTTP_429"))
                code = QString::fromStdString(act::core::errors::RATE_LIMIT);

            if (onError)
                onError(code, msg);
        });
}

void ResponsesProvider::cancel() {
    m_cancelled = true;
    if (m_network)
        m_network->cancel();
    spdlog::info("ResponsesProvider: request cancelled");
}

void ResponsesProvider::setToolDefinitions(const QList<QJsonObject> &tools) {
    m_toolDefs = tools;
}

void ResponsesProvider::setProxy(const QString &host, int port) {
    if (m_network)
        m_network->setProxy(host, port);
}

act::core::LLMMessage ResponsesProvider::toMessage(const ResponsesConverter::ParsedResponse &response) const {
    act::core::LLMMessage message;
    message.role = act::core::MessageRole::Assistant;
    message.content = response.text;
    message.toolCalls = response.toolCalls;
    if (!message.toolCalls.isEmpty())
        message.toolCall = message.toolCalls.first();
    return message;
}

} // namespace act::services