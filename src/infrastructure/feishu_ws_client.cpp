#include "infrastructure/feishu_ws_client.h"

#include <QJsonDocument>
#include <QJsonParseError>

#include <spdlog/spdlog.h>

#include <ixwebsocket/IXWebSocket.h>

namespace act::infrastructure
{

FeishuWsClient::FeishuWsClient(FeishuRestClient *restClient, QObject *parent)
    : QObject(parent)
    , m_restClient(restClient)
    , m_reconnectTimer(new QTimer(this))
    , m_pingTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &FeishuWsClient::connectInternal);

    m_pingTimer->setInterval(PING_INTERVAL_MS);
    connect(m_pingTimer, &QTimer::timeout, this, &FeishuWsClient::sendPing);
}

FeishuWsClient::~FeishuWsClient()
{
    stop();
}

bool FeishuWsClient::isConnected() const
{
    return m_connected.load();
}

void FeishuWsClient::start()
{
    m_stopping.store(false);
    m_reconnectAttempts = 0;
    connectInternal();
}

void FeishuWsClient::stop()
{
    m_stopping.store(true);
    m_reconnectTimer->stop();
    m_pingTimer->stop();

    if (m_socket)
    {
        m_socket->close();
    }

    m_connected.store(false);
    emit connectionStateChanged(false);
}

void FeishuWsClient::connectInternal()
{
    if (m_stopping.load())
        return;

    if (m_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS)
    {
        spdlog::error("FeishuWsClient: max reconnect attempts ({}) reached",
                      MAX_RECONNECT_ATTEMPTS);
        emit errorOccurred(QStringLiteral("RECONNECT_EXHAUSTED"),
                           QStringLiteral("Max reconnect attempts reached"));
        return;
    }

    // Register for SDK-mode WebSocket to get the temporary URL
    QString wsUrl;
    QString error;
    if (!m_restClient->registerWs(wsUrl, error, m_serviceId))
    {
        spdlog::error("FeishuWsClient: failed to register ws: {}", error.toStdString());
        emit errorOccurred(QStringLiteral("WS_REGISTER_ERROR"), error);
        scheduleReconnect();
        return;
    }

    // Create WebSocket and configure
    m_socket = std::make_unique<ix::WebSocket>();

    const std::string url = wsUrl.toStdString();

    m_socket->setUrl(url);
    m_socket->disablePerMessageDeflate();

    // Callbacks
    m_socket->setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
        switch (msg->type)
        {
        case ix::WebSocketMessageType::Open:
            onWsOpen();
            break;
        case ix::WebSocketMessageType::Close:
            spdlog::warn("FeishuWsClient: close code={}, reason='{}'",
                         static_cast<int>(msg->closeInfo.code),
                         msg->closeInfo.reason);
            onWsClose(static_cast<int>(msg->closeInfo.code), msg->closeInfo.reason);
            break;
        case ix::WebSocketMessageType::Message:
            onWsMessage(msg->str);
            break;
        case ix::WebSocketMessageType::Error:
            spdlog::error("FeishuWsClient: error: http_status={}, reason='{}'",
                          msg->errorInfo.http_status, msg->errorInfo.reason);
            onWsError(msg->errorInfo.http_status, msg->errorInfo.reason);
            break;
        case ix::WebSocketMessageType::Ping:
        case ix::WebSocketMessageType::Pong:
        case ix::WebSocketMessageType::Fragment:
            break;
        }
    });

    spdlog::info("FeishuWsClient: connecting to {}", url);
    m_socket->start();
}

void FeishuWsClient::onWsOpen()
{
    spdlog::info("FeishuWsClient: connected");
    m_connected.store(true);
    m_reconnectAttempts = 0;
    m_pingTimer->start();
    emit connectionStateChanged(true);
}

void FeishuWsClient::onWsClose(int code, const std::string &reason)
{
    spdlog::warn("FeishuWsClient: connection closed: code={}, reason={}",
                 code, reason);
    m_connected.store(false);
    m_pingTimer->stop();
    emit connectionStateChanged(false);

    if (!m_stopping.load())
        scheduleReconnect();
}

void FeishuWsClient::onWsMessage(const std::string &data)
{
    // Server may send plain JSON ACK responses (starts with '{')
    // alongside protobuf binary frames. Detect and skip JSON ACKs.
    if (!data.empty() && data[0] == '{')
    {
        spdlog::debug("FeishuWsClient: received JSON frame (ACK?), skipping");
        return;
    }

    // Feishu WS uses protobuf-encoded binary frames (pbbp2.Frame)
    const QByteArray raw = QByteArray::fromRawData(data.data(), static_cast<int>(data.size()));
    const auto frame = feishu::pb::decodeFrame(raw);

    if (frame.method < 0 || (frame.method != 0 && frame.method != 1))
    {
        spdlog::warn("FeishuWsClient: unknown frame method={}, raw_size={}",
                     frame.method, data.size());
        return;
    }

    if (frame.method == 0)
    {
        // Control frame — pong from server
        const auto msgType = frame.headers.value(QStringLiteral("type"));
        spdlog::info("FeishuWsClient: control frame, type={}", msgType.toStdString());
        return;
    }

    if (frame.method != 1)
    {
        spdlog::warn("FeishuWsClient: unknown frame method={}", frame.method);
        return;
    }

    // Data frame — extract payload
    const auto msgType = frame.headers.value(QStringLiteral("type"));
    if (msgType != QStringLiteral("event"))
    {
        // Not an event (could be card interaction, etc.)
        spdlog::info("FeishuWsClient: ignoring non-event frame, type={}",
                     msgType.toStdString());
        return;
    }

    if (frame.payload.isEmpty())
    {
        spdlog::warn("FeishuWsClient: empty payload in data frame");
        return;
    }

    // Parse payload as JSON (schema 2.0)
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(frame.payload, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        spdlog::warn("FeishuWsClient: payload JSON parse error: {}",
                     parseError.errorString().toStdString());
        return;
    }

    if (!doc.isObject())
    {
        spdlog::warn("FeishuWsClient: payload is not a JSON object");
        return;
    }

    const auto jsonObj = doc.object();

    auto evt = feishu::parseEvent(jsonObj);
    if (!evt)
    {
        spdlog::info("FeishuWsClient: unrecognised event format, skipping");
        return;
    }

    spdlog::info("FeishuWsClient: event received: type={}", evt->eventType.toStdString());

    // Send ACK
    sendAck(*evt);

    // Emit the event
    emit eventReceived(*evt);
}

void FeishuWsClient::onWsError(const int code, const std::string &message)
{
    spdlog::error("FeishuWsClient: error: code={}, msg={}", code, message);
    emit errorOccurred(QString::number(code),
                       QString::fromStdString(message));
}

void FeishuWsClient::sendAck(const feishu::FeishuEvent &event)
{
    if (!m_socket || !m_connected.load())
        return;

    const auto ack = feishu::buildAckResponse(event.token, event.eventId);
    const auto json = QJsonDocument(ack).toJson(QJsonDocument::Compact);

    m_socket->send(json.toStdString());
}

void FeishuWsClient::sendPing()
{
    if (!m_socket || !m_connected.load())
        return;

    const auto pingData = feishu::pb::encodePingFrame(m_serviceId);
    m_socket->sendBinary(pingData.toStdString());
    spdlog::debug("FeishuWsClient: ping sent");
}

void FeishuWsClient::scheduleReconnect()
{
    if (m_stopping.load())
        return;

    ++m_reconnectAttempts;
    // Exponential backoff: 1s, 2s, 4s, 8s, ... capped at 30s
    const int delayMs = std::min(RECONNECT_BASE_MS * (1 << (m_reconnectAttempts - 1)), 30000);

    spdlog::info("FeishuWsClient: scheduling reconnect in {}ms (attempt {})",
                 delayMs, m_reconnectAttempts);

    m_reconnectTimer->start(delayMs);
}

} // namespace act::infrastructure
