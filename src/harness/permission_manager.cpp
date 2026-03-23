#include "harness/permission_manager.h"

#include <spdlog/spdlog.h>

namespace act::harness
{

PermissionManager::PermissionManager(QObject *parent)
    : QObject(parent)
{
}

PermissionManager::Decision PermissionManager::checkPermission(
    act::core::PermissionLevel level,
    const QString &toolName,
    const QString &description,
    const QJsonObject &params)
{
    // Check deny list first
    if (isDenied(toolName))
    {
        spdlog::warn("Permission denied (deny list): {}", toolName.toStdString());
        return Decision::Denied;
    }

    // Check auto-approve
    if (isAutoApproved(level))
    {
        spdlog::info("Permission auto-approved ({}): {}",
                     static_cast<int>(level), toolName.toStdString());
        return Decision::Approved;
    }

    // Not auto-approved — in synchronous mode, deny by default
    // The async path would emit permissionRequested signal
    spdlog::info("Permission pending ({}): {} — denying in sync mode",
                 static_cast<int>(level), toolName.toStdString());
    return Decision::Denied;
}

bool PermissionManager::isAutoApproved(
    act::core::PermissionLevel level) const
{
    switch (level)
    {
    case act::core::PermissionLevel::Read:
        return m_autoApproveRead;
    case act::core::PermissionLevel::Write:
        return m_autoApproveWrite;
    case act::core::PermissionLevel::Exec:
        return m_autoApproveExec;
    case act::core::PermissionLevel::Network:
        return m_autoApproveNetwork;
    case act::core::PermissionLevel::Destructive:
        return m_autoApproveDestructive;
    }
    return false;
}

void PermissionManager::setAutoApproved(
    act::core::PermissionLevel level,
    bool enabled)
{
    switch (level)
    {
    case act::core::PermissionLevel::Read:
        m_autoApproveRead = enabled;
        break;
    case act::core::PermissionLevel::Write:
        m_autoApproveWrite = enabled;
        break;
    case act::core::PermissionLevel::Exec:
        m_autoApproveExec = enabled;
        break;
    case act::core::PermissionLevel::Network:
        m_autoApproveNetwork = enabled;
        break;
    case act::core::PermissionLevel::Destructive:
        m_autoApproveDestructive = enabled;
        break;
    }
}

void PermissionManager::addToDenyList(const QString &toolName)
{
    if (!m_denyList.contains(toolName))
    {
        m_denyList.append(toolName);
        spdlog::info("Tool added to deny list: {}", toolName.toStdString());
    }
}

bool PermissionManager::isDenied(const QString &toolName) const
{
    return m_denyList.contains(toolName);
}

} // namespace act::harness
