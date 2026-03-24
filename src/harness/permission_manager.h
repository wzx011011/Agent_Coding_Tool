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
    /// Otherwise calls user callback if set, or denies by default.
    enum class Decision
    {
        Approved,
        Denied,
        Cancelled
    };

    using ApprovalCallback = std::function<void(Decision)>;
    /// Synchronous user confirmation callback.
    /// Returns true to approve, false to deny.
    using UserPermissionCallback = std::function<bool(const act::core::PermissionRequest &)>;

    /// Check and request permission synchronously.
    [[nodiscard]] Decision checkPermission(
        act::core::PermissionLevel level,
        const QString &toolName,
        const QString &description,
        const QJsonObject &params = {});

    /// Set user confirmation callback for synchronous permission requests.
    /// Called when auto-approve is disabled for the permission level.
    /// The callback is invoked synchronously in the calling thread.
    void setPermissionCallback(UserPermissionCallback callback);

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
    UserPermissionCallback m_userCallback;
};

} // namespace act::harness
