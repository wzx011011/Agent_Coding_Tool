#pragma once

#include <QObject>
#include <QString>

#include <functional>
#include <mutex>

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
        Denied
    };

    using ApprovalCallback = std::function<void(Decision)>;
    /// Synchronous user confirmation callback.
    /// Returns true to approve, false to deny.
    using UserPermissionCallback = std::function<bool(const act::core::PermissionRequest &)>;

    /// Check and request permission synchronously.
    /// Thread-safe: acquires internal mutex.
    [[nodiscard]] Decision checkPermission(
        act::core::PermissionLevel level,
        const QString &toolName,
        const QString &description,
        const QJsonObject &params = {});

    /// Set user confirmation callback for synchronous permission requests.
    /// Thread-safe: acquires internal mutex.
    void setPermissionCallback(UserPermissionCallback callback);

    /// Auto-approve settings
    /// Thread-safe: acquires internal mutex.
    [[nodiscard]] bool isAutoApproved(act::core::PermissionLevel level) const;
    void setAutoApproved(act::core::PermissionLevel level, bool enabled);

    /// Deny list - tools/operations that are always denied
    /// Thread-safe: acquires internal mutex.
    void addToDenyList(const QString &toolName);
    void removeFromDenyList(const QString &toolName);
    [[nodiscard]] bool isDenied(const QString &toolName) const;
    [[nodiscard]] QStringList denyList() const;

    /// Number of permission levels (for iteration).
    static constexpr int kPermissionLevelCount = 5;

signals:
    void permissionRequested(const act::core::PermissionRequest &request,
                             const ApprovalCallback &callback);

private:
    mutable std::mutex m_mutex;
    bool m_autoApproveRead = true;
    bool m_autoApproveWrite = false;
    bool m_autoApproveExec = false;
    bool m_autoApproveNetwork = false;
    bool m_autoApproveDestructive = false;
    QStringList m_denyList;
    UserPermissionCallback m_userCallback;
};

} // namespace act::harness
