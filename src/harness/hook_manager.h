#pragma once

#include "harness/hook_types.h"

#include <QList>
#include <QString>

#include <mutex>

namespace act::harness
{

class HookManager
{
public:
    HookManager();

    // Registration
    void registerHook(const HookEntry &entry);
    void clearHooks();
    void loadFromConfig(const QJsonObject &hooksConfig);

    // Execution
    HookResult fireEvent(const HookContext &context) const;

    // Query
    QList<HookEntry> listHooks() const;
    int hookCount() const;

private:
    HookResult executeShellCommand(const QString &command,
                                   const HookContext &context) const;
    HookResult executeHttpCommand(const QString &url,
                                  const QString &method,
                                  const QMap<QString, QString> &headers,
                                  const HookContext &context) const;
    QString buildEnvironmentJson(const HookContext &context) const;

    QList<HookEntry> m_hooks;
    mutable std::mutex m_mutex;
};

} // namespace act::harness
