#pragma once

#include <QString>

#include "harness/interfaces.h"
#include "infrastructure/interfaces.h"

namespace act::harness
{

class NotebookEditTool : public ITool
{
public:
    explicit NotebookEditTool(act::infrastructure::IFileSystem &fs,
                              QString workspaceRoot);

    // ITool interface
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] QJsonObject schema() const override;
    act::core::ToolResult execute(const QJsonObject &params) override;
    [[nodiscard]] act::core::PermissionLevel permissionLevel() const override;
    [[nodiscard]] bool isThreadSafe() const override;

private:
    [[nodiscard]] bool isPathWithinWorkspace(
        const QString &normalizedPath) const;

    // Cell operations
    act::core::ToolResult readCell(const QString &path,
                                   const QJsonObject &params);
    act::core::ToolResult addCell(const QString &path,
                                  const QJsonObject &params);
    act::core::ToolResult deleteCell(const QString &path,
                                     const QJsonObject &params);
    act::core::ToolResult editCell(const QString &path,
                                   const QJsonObject &params);
    act::core::ToolResult listCells(const QString &path);

    // Helpers
    [[nodiscard]] bool loadNotebook(const QString &path,
                                    QJsonObject &nb) const;
    [[nodiscard]] bool saveNotebook(const QString &path,
                                    const QJsonObject &nb) const;
    [[nodiscard]] int findCellIndex(const QJsonObject &nb,
                                    const QString &cellId,
                                    int cellNumber) const;
    [[nodiscard]] QJsonObject createCell(const QString &type,
                                         const QString &source) const;

    act::infrastructure::IFileSystem &m_fs;
    QString m_workspaceRoot;

    static constexpr int kMaxCellSourceSize = 100 * 1024; // 100KB
};

} // namespace act::harness
