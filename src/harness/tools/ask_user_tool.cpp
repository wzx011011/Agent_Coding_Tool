#include "harness/tools/ask_user_tool.h"

#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

QString AskUserTool::name() const
{
    return QStringLiteral("ask_user");
}

QString AskUserTool::description() const
{
    return QStringLiteral("Ask the user a question and wait for their response. "
                          "Use this when you need clarification, "
                          "user preference, or a decision between options.");
}

QJsonObject AskUserTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("question")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("The question to ask the user");
        return obj;
    }();

    props[QStringLiteral("options")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("array");
        obj[QStringLiteral("items")] =
            QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}};
        obj[QStringLiteral("description")] =
            QStringLiteral("Optional list of choices for the user to pick from");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("question")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult AskUserTool::execute(const QJsonObject &params)
{
    auto question = params.value(QStringLiteral("question")).toString();
    if (question.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("ask_user requires 'question' parameter"));
    }

    m_prompt = question;
    m_waiting = true;

    spdlog::info("AskUserTool: waiting for user input: {}",
                 question.toStdString());

    // The actual response handling is done by the AgentLoop
    // which transitions to WaitingUserInput state.
    // This returns a marker that tells the loop to wait.
    return act::core::ToolResult::ok(
        QStringLiteral("__WAITING_USER_INPUT__"),
        {{QStringLiteral("pause_agent"), true},
         {QStringLiteral("prompt"), question}});
}

act::core::PermissionLevel AskUserTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool AskUserTool::isThreadSafe() const
{
    return false;
}

bool AskUserTool::setResponseCallback(
    std::function<void(const QString &)> callback)
{
    if (!m_waiting)
        return false;
    m_responseCallback = std::move(callback);
    return true;
}

bool AskUserTool::onUserInput(const QString &response)
{
    if (!m_waiting)
        return false;

    m_waiting = false;
    spdlog::info("AskUserTool: received user input: {}",
                 response.toStdString());

    if (m_responseCallback)
        m_responseCallback(response);

    return true;
}

} // namespace act::harness
