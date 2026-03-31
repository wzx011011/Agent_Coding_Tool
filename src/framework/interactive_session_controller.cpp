#include "framework/interactive_session_controller.h"

#include "framework/agent_loop.h"
#include <QJsonDocument>

namespace act::framework {

namespace {

QString permissionLevelToString(act::core::PermissionLevel level) {
    switch (level) {
    case act::core::PermissionLevel::Read:
        return QStringLiteral("Read");
    case act::core::PermissionLevel::Write:
        return QStringLiteral("Write");
    case act::core::PermissionLevel::Exec:
        return QStringLiteral("Exec");
    case act::core::PermissionLevel::Network:
        return QStringLiteral("Network");
    case act::core::PermissionLevel::Destructive:
        return QStringLiteral("Destructive");
    }

    return QStringLiteral("Unknown");
}

QString taskStateToString(act::core::TaskState state) {
    switch (state) {
    case act::core::TaskState::Idle:
        return QStringLiteral("Idle");
    case act::core::TaskState::Planning:
        return QStringLiteral("Planning");
    case act::core::TaskState::Running:
        return QStringLiteral("Running");
    case act::core::TaskState::WaitingApproval:
        return QStringLiteral("Waiting Approval");
    case act::core::TaskState::ToolRunning:
        return QStringLiteral("Tool Running");
    case act::core::TaskState::Completed:
        return QStringLiteral("Completed");
    case act::core::TaskState::Failed:
        return QStringLiteral("Failed");
    case act::core::TaskState::Cancelled:
        return QStringLiteral("Cancelled");
    case act::core::TaskState::Paused:
        return QStringLiteral("Paused");
    case act::core::TaskState::WaitingUserInput:
        return QStringLiteral("Waiting User Input");
    }

    return QStringLiteral("Unknown");
}

QString formatParamsPreview(const QJsonObject &params) {
    if (params.isEmpty())
        return {};

    QString preview = QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));
    if (preview.size() > 120)
        preview = preview.left(117) + QStringLiteral("...");
    return preview;
}

} // namespace

InteractiveSessionController::InteractiveSessionController(
    act::services::AIEngine &engine, act::harness::ToolRegistry &tools, act::harness::PermissionManager &permissions,
    act::harness::ContextManager &context, InteractiveSessionExecutionMode executionMode, QObject *parent)
    : QObject(parent), m_engine(engine), m_tools(tools), m_permissions(permissions), m_context(context),
      m_executionMode(executionMode) {
    m_permissions.setPermissionCallback(
        [this](const act::core::PermissionRequest &request) { return requestPermission(request); });
}

InteractiveSessionController::~InteractiveSessionController() {
    shutdownWorker();
    m_permissions.setPermissionCallback({});
}

InteractiveSessionState InteractiveSessionController::snapshotState() const {
    std::scoped_lock lock(m_stateMutex);
    return m_state;
}

bool InteractiveSessionController::isBusy() const {
    return snapshotState().isBusy();
}

void InteractiveSessionController::applyState(const std::function<void(InteractiveSessionState &)> &updater) {
    {
        std::scoped_lock lock(m_stateMutex);
        updater(m_state);
    }

    emit stateChanged();
}

void InteractiveSessionController::submitInput(const QString &message) {
    if (message.trimmed().isEmpty() || isBusy())
        return;

    applyState([&](InteractiveSessionState &state) {
        state.appendUserMessage(message);
        state.setBusy(true);
        state.setStatus(QStringLiteral("Running"));
        state.logActivity(QStringLiteral("Submitted request"));
    });

    if (m_executionMode == InteractiveSessionExecutionMode::Inline) {
        runConversationTurn(message);
        return;
    }

    shutdownWorker();
    m_workerThread = std::make_unique<std::jthread>([this, message] { runConversationTurn(message); });
}

void InteractiveSessionController::resetConversation() {
    shutdownWorker();
    applyState([](InteractiveSessionState &state) {
        state.clearConversation();
        state.appendSystemMessage(QStringLiteral("Conversation reset."));
        state.logActivity(QStringLiteral("Conversation reset"));
    });
}

void InteractiveSessionController::approvePermission() {
    resolvePermission(true);
}

void InteractiveSessionController::denyPermission() {
    resolvePermission(false);
}

bool InteractiveSessionController::requestPermission(const act::core::PermissionRequest &request) {
    {
        QMutexLocker locker(&m_permissionMutex);
        m_permissionWaiting = true;
        m_permissionResolved = false;
        m_permissionApproved = false;
    }

    applyState([&](InteractiveSessionState &state) {
        state.setPermissionPrompt(request.toolName, permissionLevelToString(request.level), request.description);
        state.logActivity(QStringLiteral("Permission requested for %1").arg(request.toolName));
        state.setStatus(QStringLiteral("Waiting Approval"));
    });

    QMutexLocker locker(&m_permissionMutex);
    while (!m_permissionResolved && !m_shuttingDown.load())
        m_permissionWaitCondition.wait(&m_permissionMutex);

    const bool approved = m_permissionResolved && m_permissionApproved;
    m_permissionWaiting = false;
    locker.unlock();

    applyState([&](InteractiveSessionState &state) {
        state.clearPermissionPrompt();
        state.logActivity(QStringLiteral("Permission %1 for %2")
                              .arg(approved ? QStringLiteral("approved") : QStringLiteral("denied"), request.toolName));
        state.setStatus(QStringLiteral("Running"));
    });

    return approved;
}

void InteractiveSessionController::resolvePermission(bool approved) {
    QMutexLocker locker(&m_permissionMutex);
    if (!m_permissionWaiting)
        return;

    m_permissionApproved = approved;
    m_permissionResolved = true;
    m_permissionWaitCondition.wakeAll();
}

void InteractiveSessionController::runConversationTurn(const QString &message) {
    auto workerEngine = m_executionMode == InteractiveSessionExecutionMode::AsyncThreaded
                            ? m_engine.createDetachedInstance()
                            : nullptr;
    act::services::AIEngine &activeEngine = workerEngine ? *workerEngine : m_engine;

    QMetaObject::Connection streamConnection =
        QObject::connect(&activeEngine, &act::services::AIEngine::streamTokenReceived, [this](const QString &token) {
            applyState([&](InteractiveSessionState &state) { state.appendAssistantToken(token); });
            emit tokenStreamed(token);
        });

    AgentLoop loop(activeEngine, m_tools, m_permissions, m_context);
    loop.setMaxTurns(50);
    loop.setSystemPrompt(m_systemPrompt);
    loop.setEventCallback([this](const act::core::RuntimeEvent &event) {
        switch (event.type) {
        case act::core::EventType::ToolCallStarted: {
            const QString toolName = event.data.value(QStringLiteral("tool")).toString();
            const auto params = event.data.value(QStringLiteral("params")).toObject();
            applyState([&](InteractiveSessionState &state) {
                state.logActivity(QStringLiteral("Tool start: %1 %2").arg(toolName, formatParamsPreview(params)));
                state.setStatus(QStringLiteral("Tool Running"));
            });
            break;
        }
        case act::core::EventType::ToolCallCompleted: {
            const QString toolName = event.data.value(QStringLiteral("tool")).toString();
            const bool success = event.data.value(QStringLiteral("success")).toBool();
            const QString output = event.data.value(QStringLiteral("output")).toString();
            const QString errorCode = event.data.value(QStringLiteral("error_code")).toString();
            applyState([&](InteractiveSessionState &state) {
                state.logActivity(QStringLiteral("Tool %1: %2")
                                      .arg(success ? QStringLiteral("ok") : QStringLiteral("failed"), toolName));
                if (success && !output.isEmpty())
                    state.appendMessage(SessionMessageKind::Tool, toolName, output);
                else if (!success)
                    state.appendErrorMessage(QStringLiteral("%1 [%2]").arg(toolName, errorCode));
            });
            break;
        }
        case act::core::EventType::TaskStateChanged: {
            const auto stateValue =
                static_cast<act::core::TaskState>(event.data.value(QStringLiteral("state")).toInt());
            applyState([&](InteractiveSessionState &state) { state.setStatus(taskStateToString(stateValue)); });
            break;
        }
        case act::core::EventType::ErrorOccurred: {
            const QString code = event.data.value(QStringLiteral("code")).toString();
            const QString errorMessage = event.data.value(QStringLiteral("message")).toString();
            applyState([&](InteractiveSessionState &state) {
                state.appendErrorMessage(QStringLiteral("[%1] %2").arg(code, errorMessage));
                state.setStatus(QStringLiteral("Failed"));
            });
            break;
        }
        default:
            break;
        }
    });

    loop.submitUserMessage(message);
    QObject::disconnect(streamConnection);

    QString fallbackAssistant;
    for (auto it = loop.messages().crbegin(); it != loop.messages().crend(); ++it) {
        if (it->role == act::core::MessageRole::Assistant && !it->content.isEmpty()) {
            fallbackAssistant = it->content;
            break;
        }
    }

    const auto finalState = loop.state();
    applyState([&](InteractiveSessionState &state) {
        state.finalizeAssistantMessage(fallbackAssistant);
        state.setBusy(false);
        state.setStatus(taskStateToString(finalState));
        if (finalState == act::core::TaskState::Completed)
            state.logActivity(QStringLiteral("Turn completed"));
        else if (finalState == act::core::TaskState::Cancelled)
            state.logActivity(QStringLiteral("Turn cancelled"));
    });

    emit turnCompleted();
}

void InteractiveSessionController::shutdownWorker() {
    m_shuttingDown = true;
    {
        QMutexLocker locker(&m_permissionMutex);
        m_permissionResolved = true;
        m_permissionApproved = false;
        m_permissionWaitCondition.wakeAll();
    }

    m_engine.cancel();

    if (m_workerThread) {
        if (m_workerThread->joinable())
            m_workerThread->join();
        m_workerThread.reset();
    }

    m_shuttingDown = false;
}

} // namespace act::framework