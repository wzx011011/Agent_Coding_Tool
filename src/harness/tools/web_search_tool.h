#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/http_network.h"

namespace act::harness
{

class WebSearchTool : public ITool
{
public:
    explicit WebSearchTool(act::infrastructure::HttpNetwork &http);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

    static constexpr int MAX_RESULTS = 10;
    static constexpr int SEARCH_TIMEOUT_SECONDS = 15;

private:
    act::infrastructure::HttpNetwork &m_http;
};

} // namespace act::harness
