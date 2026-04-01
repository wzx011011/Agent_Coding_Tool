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
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check deny list first
    if (m_denyList.contains(toolName))
    {
        spdlog::warn("Permission denied (deny list): {}", toolName.toStdString());
        return Decision::Denied;
    }

    // Check auto-approve
    bool autoApproved = false;
    switch (level)
    {
    case act::core::PermissionLevel::Read:
        autoApproved = m_autoApproveRead;
        break;
    case act::core::PermissionLevel::Write:
        autoApproved = m_autoApproveWrite;
        break;
    case act::core::PermissionLevel::Exec:
        autoApproved = m_autoApproveExec;
        break;
    case act::core::PermissionLevel::Network:
        autoApproved = m_autoApproveNetwork;
        break;
    case act::core::PermissionLevel::Destructive:
        autoApproved = m_autoApproveDestructive;
        break;
    }

    if (autoApproved)
    {
        spdlog::info("Permission auto-approved ({}): {}",
                     static_cast<int>(level), toolName.toStdString());
        return Decision::Approved;
    }

    // Not auto-approved — call user callback if set
    if (m_userCallback)
    {
        act::core::PermissionRequest request;
        request.level = level;
        request.toolName = toolName;
        request.description = description;
        request.params = params;

        spdlog::info("Permission requesting user confirmation ({}): {}",
                     static_cast<int>(level), toolName.toStdString());

        // Unlock during callback to avoid deadlock if callback
        // calls back into PermissionManager
        m_mutex.unlock();
        bool approved = m_userCallback(request);
        m_mutex.lock();

        return approved ? Decision::Approved : Decision::Denied;
    }

    // No callback set — deny by default
    spdlog::info("Permission denied (no callback, level {}): {}",
                 static_cast<int>(level), toolName.toStdString());
    return Decision::Denied;
}

void PermissionManager::setPermissionCallback(UserPermissionCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_userCallback = std::move(callback);
}

bool PermissionManager::isAutoApproved(
    act::core::PermissionLevel level) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
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
    std::lock_guard<std::mutex> lock(m_mutex);
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
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_denyList.contains(toolName))
    {
        m_denyList.append(toolName);
        spdlog::info("Tool added to deny list: {}", toolName.toStdString());
    }
}

bool PermissionManager::isDenied(const QString &toolName) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_denyList.contains(toolName);
}

void PermissionManager::removeFromDenyList(const QString &toolName)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_denyList.removeOne(toolName))
    {
        spdlog::info("Tool removed from deny list: {}", toolName.toStdString());
    }
}

QStringList PermissionManager::denyList() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_denyList;
}

} // namespace act::harness
