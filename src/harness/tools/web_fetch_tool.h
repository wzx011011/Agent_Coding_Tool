#pragma once

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

private:
    static bool isTextContentType(const QString &contentType);

    act::infrastructure::HttpNetwork &m_http;
};

} // namespace act::harness
