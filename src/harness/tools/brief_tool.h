#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "services/interfaces.h"

namespace act::harness
{

class BriefTool : public ITool
{
public:
    explicit BriefTool(act::services::IAIEngine &engine);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    act::core::ToolResult execute(const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

    static constexpr int DEFAULT_MAX_TOKENS = 200;
    static constexpr int TIMEOUT_SECONDS = 30;
    static constexpr int MIN_CONTENT_LENGTH = 50;

private:
    act::services::IAIEngine &m_engine;
};

} // namespace act::harness
