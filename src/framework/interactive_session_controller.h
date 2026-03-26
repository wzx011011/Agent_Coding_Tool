#pragma once

#include "framework/interactive_session_state.h"
#include "harness/context_manager.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "services/ai_engine.h"
#include <QMutex>
#include <QObject>
#include <QWaitCondition>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace act::framework {

enum class InteractiveSessionExecutionMode {
    AsyncThreaded,
    Inline,
};

class InteractiveSessionController : public QObject {
    Q_OBJECT

  public:
    InteractiveSessionController(
        act::services::AIEngine &engine, act::harness::ToolRegistry &tools,
        act::harness::PermissionManager &permissions, act::harness::ContextManager &context,
        InteractiveSessionExecutionMode executionMode = InteractiveSessionExecutionMode::AsyncThreaded,
        QObject *parent = nullptr);
    ~InteractiveSessionController() override;

    [[nodiscard]] InteractiveSessionState snapshotState() const;
    [[nodiscard]] bool isBusy() const;

    void submitInput(const QString &message);
    void resetConversation();
    void approvePermission();
    void denyPermission();

  signals:
    void stateChanged();

    /// Emitted when a streaming token arrives (for terminal observation).
    void tokenStreamed(const QString &token);

    /// Emitted when a conversation turn completes (for terminal newline).
    void turnCompleted();

  private:
    void applyState(const std::function<void(InteractiveSessionState &)> &updater);
    bool requestPermission(const act::core::PermissionRequest &request);
    void resolvePermission(bool approved);
    void runConversationTurn(const QString &message);
    void shutdownWorker();

    act::services::AIEngine &m_engine;
    act::harness::ToolRegistry &m_tools;
    act::harness::PermissionManager &m_permissions;
    act::harness::ContextManager &m_context;

    mutable std::mutex m_stateMutex;
    InteractiveSessionState m_state;

    QMutex m_permissionMutex;
    QWaitCondition m_permissionWaitCondition;
    bool m_permissionWaiting = false;
    bool m_permissionResolved = false;
    bool m_permissionApproved = false;

    InteractiveSessionExecutionMode m_executionMode;
    std::unique_ptr<std::jthread> m_workerThread;
    std::atomic<bool> m_shuttingDown{false};
};

} // namespace act::framework