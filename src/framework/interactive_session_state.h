#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace act::framework {

enum class SessionMessageKind {
    User,
    Assistant,
    System,
    Tool,
    Error,
};

struct SessionMessage {
    SessionMessageKind kind = SessionMessageKind::System;
    QString title;
    QString content;
};

struct PermissionPromptState {
    bool active = false;
    QString toolName;
    QString level;
    QString description;
};

class InteractiveSessionState {
  public:
    void appendMessage(SessionMessageKind kind, const QString &title, const QString &content);
    void appendUserMessage(const QString &content);
    void appendSystemMessage(const QString &content);
    void appendErrorMessage(const QString &content);
    void appendAssistantToken(const QString &token);
    void finalizeAssistantMessage(const QString &fallbackContent = {});

    void logActivity(const QString &entry);
    void clearActivity();

    void setStatus(const QString &status);
    [[nodiscard]] const QString &status() const { return m_status; }

    void setBusy(bool busy);
    [[nodiscard]] bool isBusy() const { return m_busy; }

    void setPermissionPrompt(const QString &toolName, const QString &level, const QString &description);
    void clearPermissionPrompt();
    [[nodiscard]] const PermissionPromptState &permissionPrompt() const { return m_permissionPrompt; }

    void clearConversation();

    [[nodiscard]] const QList<SessionMessage> &messages() const { return m_messages; }
    [[nodiscard]] const QStringList &activityLog() const { return m_activityLog; }

  private:
    QList<SessionMessage> m_messages;
    QStringList m_activityLog;
    QString m_status = QStringLiteral("Idle");
    PermissionPromptState m_permissionPrompt;
    bool m_busy = false;
    bool m_assistantStreaming = false;
};

} // namespace act::framework