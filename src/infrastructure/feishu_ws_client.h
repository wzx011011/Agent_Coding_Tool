#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <atomic>
#include <memory>
#include <string>

#include "infrastructure/feishu_protocol.h"
#include "infrastructure/feishu_rest_client.h"

namespace ix
{
class WebSocket;
}

namespace act::infrastructure
{

/// Feishu WebSocket client for SDK-mode event subscription.
/// Connects via protobuf binary protocol to msg-frontier.feishu.cn,
/// receives events, sends ACKs, handles heartbeat and reconnection.
class FeishuWsClient : public QObject
{
    Q_OBJECT

public:
    explicit FeishuWsClient(FeishuRestClient *restClient,
                            QObject *parent = nullptr);
    ~FeishuWsClient() override;

    void start();
    void stop();
    [[nodiscard]] bool isConnected() const;

signals:
    /// A Feishu event was received and parsed.
    void eventReceived(const feishu::FeishuEvent &event);

    /// Connection state changed.
    void connectionStateChanged(bool connected);

    /// An error occurred (connection, protocol, etc.).
    void errorOccurred(const QString &errorCode, const QString &errorMessage);

private:
    void connectInternal();
    void onWsOpen();
    void onWsClose(int code, const std::string &reason);
    void onWsMessage(const std::string &data);
    void onWsError(const int code, const std::string &message);
    void sendAck(const feishu::FeishuEvent &event);
    void sendPing();
    void scheduleReconnect();

    FeishuRestClient *m_restClient;
    std::unique_ptr<ix::WebSocket> m_socket;
    QTimer *m_reconnectTimer;
    QTimer *m_pingTimer;
    int32_t m_serviceId = 0;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_stopping{false};
    int m_reconnectAttempts = 0;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int RECONNECT_BASE_MS = 1000;
    static constexpr int PING_INTERVAL_MS = 120000; // 2 minutes default
};

} // namespace act::infrastructure
