#include "harness/tools/brief_tool.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QTimer>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"
#include "core/types.h"

namespace act::harness
{

BriefTool::BriefTool(act::services::IAIEngine &engine)
    : m_engine(engine)
{
}

QString BriefTool::name() const
{
    return QStringLiteral("brief");
}

QString BriefTool::description() const
{
    return QStringLiteral(
        "Compress long text into a short summary. "
        "Useful for condensing large outputs before passing to sub-agents.");
}

QJsonObject BriefTool::schema() const
{
    QJsonObject contentProp;
    contentProp[QStringLiteral("type")] = QStringLiteral("string");
    contentProp[QStringLiteral("description")] =
        QStringLiteral("Text to compress");

    QJsonObject maxTokensProp;
    maxTokensProp[QStringLiteral("type")] = QStringLiteral("integer");
    maxTokensProp[QStringLiteral("description")] =
        QStringLiteral("Max output tokens (default 200)");

    QJsonObject props;
    props[QStringLiteral("content")] = contentProp;
    props[QStringLiteral("max_tokens")] = maxTokensProp;

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("content")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult BriefTool::execute(const QJsonObject &params)
{
    // Validate content parameter
    const auto contentValue = params.value(QStringLiteral("content"));
    if (!contentValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'content' parameter must be a string"));
    }

    const QString content = contentValue.toString();
    if (content.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'content' parameter must not be empty"));
    }

    // Short content is returned as-is
    if (content.length() < MIN_CONTENT_LENGTH)
    {
        return act::core::ToolResult::ok(content);
    }

    // Parse max_tokens
    const int maxTokens =
        params.value(QStringLiteral("max_tokens"))
            .toInt(DEFAULT_MAX_TOKENS);

    // Build summarization prompt
    const QString prompt =
        QStringLiteral(
            "Summarize the following in under %1 tokens. "
            "Preserve key facts, decisions, and action items:\n\n%2")
            .arg(maxTokens)
            .arg(content);

    // Prepare the message for the LLM
    const QList<act::core::LLMMessage> messages = {
        {.role = act::core::MessageRole::User, .content = prompt}};

    // Synchronous call via local event loop
    QString summary;
    bool success = false;
    bool completedSynchronously = false;
    QString errorCode;
    QString errorMessage;

    QEventLoop *loopPtr = nullptr;
    QTimer *timerPtr = nullptr;

    // Set up event loop and timer only if QCoreApplication exists,
    // since async engines need it. Sync engines (mocks) complete immediately.
    const bool hasApp = (QCoreApplication::instance() != nullptr);

    if (hasApp)
    {
        loopPtr = new QEventLoop;
        timerPtr = new QTimer;
        timerPtr->setSingleShot(true);

        QObject::connect(timerPtr, &QTimer::timeout, loopPtr,
                         [&errorCode, &errorMessage, &success, loopPtr]() {
                             errorCode = act::core::errors::TIMEOUT;
                             errorMessage =
                                 QStringLiteral("BriefTool timed out after %1 seconds")
                                     .arg(TIMEOUT_SECONDS);
                             success = false;
                             loopPtr->quit();
                         });
    }

    auto onComplete = [&completedSynchronously, loopPtr]() {
        if (loopPtr)
            loopPtr->quit();
        else
            completedSynchronously = true;
    };

    m_engine.chat(
        messages,
        [&summary, &success, &completedSynchronously, loopPtr](
            act::core::LLMMessage msg) {
            summary = msg.content;
            success = true;
            if (loopPtr)
                loopPtr->quit();
            else
                completedSynchronously = true;
        },
        onComplete,
        [&errorCode, &errorMessage, &success, &completedSynchronously,
         loopPtr](QString code, QString msg) {
            errorCode = code;
            errorMessage = msg;
            success = false;
            if (loopPtr)
                loopPtr->quit();
            else
                completedSynchronously = true;
        });

    if (hasApp && !completedSynchronously)
    {
        timerPtr->start(TIMEOUT_SECONDS * 1000);
        loopPtr->exec();
        timerPtr->stop();
    }

    delete timerPtr;
    delete loopPtr;

    if (!success)
    {
        spdlog::warn("BriefTool: summarization failed: {}",
                     errorMessage.toStdString());
        return act::core::ToolResult::err(
            errorCode.isEmpty() ? act::core::errors::PROVIDER_TIMEOUT
                                : errorCode,
            errorMessage.isEmpty()
                ? QStringLiteral("Summarization failed")
                : errorMessage);
    }

    return act::core::ToolResult::ok(summary);
}

act::core::PermissionLevel BriefTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool BriefTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
