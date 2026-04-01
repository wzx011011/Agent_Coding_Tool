#pragma once

#include <QJsonObject>
#include <QString>

#include "harness/interfaces.h"

namespace act::harness {

/// Enhanced AskUserTool with multi-question, options, preview support.
/// This is a READ-ONLY tool -- it validates and structures the questions
/// for the LLM. The actual user interaction is handled by the interactive
/// permission flow, not this tool.
class AskUserV2Tool : public ITool {
public:
    explicit AskUserV2Tool() = default;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    act::core::ToolResult execute(const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;
};

} // namespace act::harness
