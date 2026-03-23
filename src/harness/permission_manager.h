#pragma once

#include <QObject>
#include <QString>

#include <functional>

#include "core/enums.h"
#include "core/types.h"

namespace act::harness
{

class PermissionManager : public QObject
{
    Q_OBJECT
public:
    explicit PermissionManager(QObject *parent = nullptr);

    /// Request permission for a tool execution.
    /// If auto-approve is enabled for the level, returns immediately.
    /// Otherwise emits permissionRequested signal and waits.
    enum class Decision
    {
        Approved,
        Denied,
        Cancelled
    };

    using ApprovalCallback = std::function<void(Decision)>;

    /// Check and request permission synchronously (for auto-approve).
    [[nodiscard]] Decision checkPermission(
        act::core::PermissionLevel level,
        const QString &toolName,
        const QString &description,
        const QJsonObject &params = {});

    /// Auto-approve settings
    [[nodiscard]] bool isAutoApproved(act::core::PermissionLevel level) const;
    void setAutoApproved(act::core::PermissionLevel level, bool enabled);

    /// Deny list - tools/operations that are always denied
    void addToDenyList(const QString &toolName);
    [[nodiscard]] bool isDenied(const QString &toolName) const;

signals:
    void permissionRequested(const act::core::PermissionRequest &request,
                             const ApprovalCallback &callback);

private:
    bool m_autoApproveRead = true;
    bool m_autoApproveWrite = false;
    bool m_autoApproveExec = false;
    bool m_autoApproveNetwork = false;
    bool m_autoApproveDestructive = false;
    QStringList m_denyList;
};

} // namespace act::harness
