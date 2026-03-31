#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "harness/interfaces.h"

namespace act::harness
{

/// In-memory task list tool for managing a simple todo list.
/// Supports add, remove, complete, list, and clear actions.
class TodoWriteTool : public ITool
{
public:
    /// Status of a todo item.
    enum class Status
    {
        Pending,
        InProgress,
        Completed
    };

    /// A single todo item.
    struct TodoItem
    {
        QString subject;
        QString description;
        Status status = Status::Pending;
        QDateTime createdAt;
    };

    explicit TodoWriteTool() = default;

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    [[nodiscard]] act::core::ToolResult execute(
        const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

    /// Get the current todo list (for testing).
    [[nodiscard]] const QList<TodoItem> &items() const { return m_items; }

    /// Get the number of items.
    [[nodiscard]] int count() const { return m_items.size(); }

    /// Parse a status string to enum.
    [[nodiscard]] static std::optional<Status> parseStatus(const QString &str);

    /// Convert a status enum to display string.
    [[nodiscard]] static QString statusToString(Status status);

private:
    [[nodiscard]] act::core::ToolResult handleAdd(const QJsonObject &params);
    [[nodiscard]] act::core::ToolResult handleRemove(const QJsonObject &params);
    [[nodiscard]] act::core::ToolResult handleComplete(const QJsonObject &params);
    [[nodiscard]] act::core::ToolResult handleList();
    [[nodiscard]] act::core::ToolResult handleClear();

    QList<TodoItem> m_items;
};

} // namespace act::harness
