#pragma once

#include <QString>

#include "harness/interfaces.h"

namespace act::harness
{

/// Read-only tool that parses compiler output into structured diagnostics.
/// Supports MSVC, GCC, and Clang error/warning formats.
class DiagnosticTool : public ITool
{
public:
    explicit DiagnosticTool() = default;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    [[nodiscard]] static QJsonArray parseDiagnostics(const QString &output);
};

} // namespace act::harness
