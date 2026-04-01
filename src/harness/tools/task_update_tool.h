#pragma once

#include <QJsonObject>
#include <QString>

#include "harness/interfaces.h"
#include "harness/task_manager.h"

namespace act::harness
{

class TaskUpdateTool : public ITool
{
public:
    explicit TaskUpdateTool(TaskManager &manager);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    TaskManager &m_manager;
};

} // namespace act::harness
