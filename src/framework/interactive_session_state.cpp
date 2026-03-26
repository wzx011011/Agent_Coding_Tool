#include "framework/interactive_session_state.h"

namespace act::framework {

namespace {

QString defaultTitle(SessionMessageKind kind) {
    switch (kind) {
    case SessionMessageKind::User:
        return QStringLiteral("User");
    case SessionMessageKind::Assistant:
        return QStringLiteral("Assistant");
    case SessionMessageKind::System:
        return QStringLiteral("System");
    case SessionMessageKind::Tool:
        return QStringLiteral("Tool");
    case SessionMessageKind::Error:
        return QStringLiteral("Error");
    }

    return QStringLiteral("Message");
}

} // namespace

void InteractiveSessionState::appendMessage(SessionMessageKind kind, const QString &title, const QString &content) {
    m_messages.append(SessionMessage{kind, title.isEmpty() ? defaultTitle(kind) : title, content});
    m_assistantStreaming = false;
}

void InteractiveSessionState::appendUserMessage(const QString &content) {
    appendMessage(SessionMessageKind::User, QStringLiteral("You"), content);
}

void InteractiveSessionState::appendSystemMessage(const QString &content) {
    appendMessage(SessionMessageKind::System, QStringLiteral("System"), content);
}

void InteractiveSessionState::appendErrorMessage(const QString &content) {
    appendMessage(SessionMessageKind::Error, QStringLiteral("Error"), content);
}

void InteractiveSessionState::appendAssistantToken(const QString &token) {
    if (!m_assistantStreaming || m_messages.isEmpty() || m_messages.last().kind != SessionMessageKind::Assistant) {
        m_messages.append(SessionMessage{SessionMessageKind::Assistant, QStringLiteral("Assistant"), token});
        m_assistantStreaming = true;
        return;
    }

    m_messages.last().content += token;
}

void InteractiveSessionState::finalizeAssistantMessage(const QString &fallbackContent) {
    if (m_assistantStreaming) {
        if (m_messages.last().content.isEmpty() && !fallbackContent.isEmpty())
            m_messages.last().content = fallbackContent;
        m_assistantStreaming = false;
        return;
    }

    if (!fallbackContent.isEmpty())
        m_messages.append(SessionMessage{SessionMessageKind::Assistant, QStringLiteral("Assistant"), fallbackContent});
}

void InteractiveSessionState::logActivity(const QString &entry) {
    if (!entry.isEmpty())
        m_activityLog.append(entry);
}

void InteractiveSessionState::clearActivity() {
    m_activityLog.clear();
}

void InteractiveSessionState::setStatus(const QString &status) {
    m_status = status;
}

void InteractiveSessionState::setBusy(bool busy) {
    m_busy = busy;
}

void InteractiveSessionState::setPermissionPrompt(const QString &toolName, const QString &level,
                                                  const QString &description) {
    m_permissionPrompt.active = true;
    m_permissionPrompt.toolName = toolName;
    m_permissionPrompt.level = level;
    m_permissionPrompt.description = description;
}

void InteractiveSessionState::clearPermissionPrompt() {
    m_permissionPrompt = {};
}

void InteractiveSessionState::clearConversation() {
    m_messages.clear();
    m_activityLog.clear();
    m_status = QStringLiteral("Idle");
    m_permissionPrompt = {};
    m_busy = false;
    m_assistantStreaming = false;
}

} // namespace act::framework