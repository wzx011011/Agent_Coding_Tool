#include "infrastructure/mcp_transport.h"

#include <QJsonDocument>
#include <QProcess>

#include <spdlog/spdlog.h>

namespace act::infrastructure {

// ---------------------------------------------------------------------------
// McpTransport (base)
// ---------------------------------------------------------------------------

void McpTransport::setNotificationHandler(NotificationHandler handler)
{
    m_notificationHandler = std::move(handler);
}

// ---------------------------------------------------------------------------
// McpStdioTransport
// ---------------------------------------------------------------------------

McpStdioTransport::McpStdioTransport(const QString &command,
                                      const QStringList &args,
                                      const QMap<QString, QString> &env)
    : m_command(command)
    , m_args(args)
    , m_env(env)
{
}

McpStdioTransport::~McpStdioTransport()
{
    close();
}

bool McpStdioTransport::connect()
{
    if (m_connected)
    {
        spdlog::warn("McpStdioTransport: already connected");
        return true;
    }

    if (m_command.isEmpty())
    {
        spdlog::error("McpStdioTransport: command is empty");
        return false;
    }

    m_process = new QProcess();

    // Set custom environment variables if provided.
    if (!m_env.isEmpty())
    {
        auto envObj = QProcessEnvironment::systemEnvironment();
        for (auto it = m_env.constBegin(); it != m_env.constEnd(); ++it)
            envObj.insert(it.key(), it.value());
        m_process->setProcessEnvironment(envObj);
    }

    // Forward stderr to our log for diagnostics.
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    // Start the process.
    m_process->start(m_command, m_args);
    if (!m_process->waitForStarted(3000))
    {
        spdlog::error("McpStdioTransport: failed to start process '{}': {}",
                      m_command.toStdString(),
                      m_process->errorString().toStdString());
        delete m_process;
        m_process = nullptr;
        return false;
    }

    m_connected = true;

    // Launch the read loop in a background thread.
    // We use Qt signals to read available data from stdout.
    // For simplicity, we read synchronously in a dedicated fashion:
    // each sendRequest call will read until it finds the matching response.
    spdlog::info("McpStdioTransport: connected to '{}'", m_command.toStdString());
    return true;
}

QJsonObject McpStdioTransport::sendRequest(const QString &method,
                                             const QJsonObject &params)
{
    if (!m_connected || !m_process)
    {
        spdlog::error("McpStdioTransport: not connected");
        return {};
    }

    int requestId = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        requestId = ++m_nextId;
    }

    QJsonObject request;
    request[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    request[QStringLiteral("id")] = requestId;
    request[QStringLiteral("method")] = method;
    if (!params.isEmpty())
        request[QStringLiteral("params")] = params;

    writeMessage(request);

    return waitForResponse(requestId, kDefaultTimeoutMs);
}

void McpStdioTransport::close()
{
    if (!m_process)
        return;

    m_connected = false;

    // Attempt graceful shutdown.
    if (m_process->state() != QProcess::NotRunning)
    {
        m_process->closeWriteChannel();
        m_process->waitForFinished(2000);
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
    }

    delete m_process;
    m_process = nullptr;

    // Wake anyone still waiting.
    m_cv.notify_all();

    spdlog::info("McpStdioTransport: closed");
}

bool McpStdioTransport::isConnected() const
{
    return m_connected && m_process &&
           m_process->state() == QProcess::Running;
}

void McpStdioTransport::writeMessage(const QJsonObject &msg)
{
    if (!m_process)
        return;

    QJsonDocument doc(msg);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + '\n';

    m_process->write(data);
    m_process->waitForBytesWritten(3000);
}

QJsonObject McpStdioTransport::waitForResponse(int id, int timeoutMs)
{
    // Try to read and parse responses from stdout until we find the one
    // matching our request id, or we time out.
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeoutMs);

    while (true)
    {
        // Check if we already have the response.
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_responseMap.contains(id))
            {
                auto resp = m_responseMap.take(id);
                return resp;
            }
        }

        // Check remaining time.
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
        {
            spdlog::error("McpStdioTransport: timed out waiting for response "
                          "id={}",
                          id);
            break;
        }

        // Try to read data from the process.
        if (!m_process || m_process->state() == QProcess::NotRunning)
        {
            spdlog::error(
                "McpStdioTransport: process exited while waiting for response");
            break;
        }

        // Wait for data with a small timeout.
        bool ready = m_process->waitForReadyRead(100);
        if (ready)
        {
            m_buffer.append(m_process->readAllStandardOutput());

            // Parse complete JSON-RPC messages (newline-delimited).
            while (true)
            {
                int newlinePos = m_buffer.indexOf('\n');
                if (newlinePos < 0)
                    break;

                QByteArray line = m_buffer.left(newlinePos).trimmed();
                m_buffer.remove(0, newlinePos + 1);

                if (line.isEmpty())
                    continue;

                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
                if (parseError.error != QJsonParseError::NoError)
                {
                    spdlog::warn("McpStdioTransport: failed to parse JSON: {}",
                                 parseError.errorString().toStdString());
                    continue;
                }

                if (!doc.isObject())
                {
                    spdlog::warn(
                        "McpStdioTransport: received non-object JSON message");
                    continue;
                }

                QJsonObject msg = doc.object();

                // Check if this is a notification (no id field).
                if (!msg.contains(QStringLiteral("id")))
                {
                    if (m_notificationHandler)
                        m_notificationHandler(msg);
                    continue;
                }

                int msgId = msg.value(QStringLiteral("id")).toInt(-1);
                if (msgId >= 0)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_responseMap.insert(msgId, msg);
                }
            }
        }
    }

    return {};
}

} // namespace act::infrastructure
