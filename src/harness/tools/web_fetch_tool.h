#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/http_network.h"

namespace act::harness
{

class WebFetchTool : public ITool
{
public:
    explicit WebFetchTool(act::infrastructure::HttpNetwork &http);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

    static constexpr int MAX_RESPONSE_SIZE = 50 * 1024; // 50KB
    static constexpr int FETCH_TIMEOUT_SECONDS = 15;

private:

    /// Check if a URL uses an allowed scheme (http or https).
    [[nodiscard]] static bool isAllowedScheme(const QString &url);

    /// Check if an IPv4 address (as 4 octets) is a private/reserved range.
    /// Blocks loopback, link-local, RFC 1918, and cloud metadata endpoints.
    [[nodiscard]] static bool isPrivateIPv4(quint8 a, quint8 b, quint8 c, quint8 d);

    act::infrastructure::HttpNetwork &m_http;
};

} // namespace act::harness
