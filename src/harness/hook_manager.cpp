#include "harness/hook_manager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>

#include <spdlog/spdlog.h>

namespace act::harness
{

HookManager::HookManager() = default;

void HookManager::registerHook(const HookEntry &entry)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hooks.append(entry);
    spdlog::info("HookManager: registered hook for event type {}",
                 static_cast<int>(entry.eventType));
}

void HookManager::clearHooks()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hooks.clear();
    spdlog::info("HookManager: all hooks cleared");
}

void HookManager::loadFromConfig(const QJsonObject &hooksConfig)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    static const QMap<QString, HookEventType> kEventTypeMap = {
        {QStringLiteral("PreToolUse"), HookEventType::PreToolUse},
        {QStringLiteral("PostToolUse"), HookEventType::PostToolUse},
        {QStringLiteral("UserPromptSubmit"), HookEventType::UserPromptSubmit},
        {QStringLiteral("Notification"), HookEventType::Notification},
        {QStringLiteral("SessionStart"), HookEventType::SessionStart},
        {QStringLiteral("SessionStop"), HookEventType::SessionStop},
    };

    for (auto it = hooksConfig.constBegin(); it != hooksConfig.constEnd(); ++it)
    {
        QString eventTypeStr = it.key();
        HookEventType eventType = HookEventType::PreToolUse;
        if (auto mapIt = kEventTypeMap.find(eventTypeStr);
            mapIt != kEventTypeMap.end())
        {
            eventType = mapIt.value();
        }
        else
        {
            spdlog::warn("HookManager: unknown event type '{}', skipping",
                         eventTypeStr.toStdString());
            continue;
        }

        const QJsonArray entries = it.value().toArray();
        for (const QJsonValue &entryVal : entries)
        {
            QJsonObject entryObj = entryVal.toObject();
            HookEntry entry;
            entry.eventType = eventType;
            entry.matcher = entryObj[QStringLiteral("matcher")].toString();

            const QJsonArray hooks = entryObj[QStringLiteral("hooks")].toArray();
            for (const QJsonValue &hookVal : hooks)
            {
                QJsonObject hookObj = hookVal.toObject();
                HookCommand cmd;
                QString typeStr = hookObj[QStringLiteral("type")].toString();

                if (typeStr == QStringLiteral("bash") ||
                    typeStr == QStringLiteral("shell"))
                {
                    cmd.type = HookCommandType::Shell;
                    cmd.command = hookObj[QStringLiteral("command")].toString();
                }
                else if (typeStr == QStringLiteral("prompt"))
                {
                    cmd.type = HookCommandType::Prompt;
                    cmd.command = hookObj[QStringLiteral("prompt")].toString();
                }
                else if (typeStr == QStringLiteral("http"))
                {
                    cmd.type = HookCommandType::Http;
                    cmd.command = hookObj[QStringLiteral("url")].toString();
                    cmd.method = hookObj[QStringLiteral("method")].toString(
                        QStringLiteral("POST"));
                    const QJsonObject headersObj =
                        hookObj[QStringLiteral("headers")].toObject();
                    for (auto hIt = headersObj.constBegin();
                         hIt != headersObj.constEnd(); ++hIt)
                    {
                        cmd.headers.insert(hIt.key(), hIt.value().toString());
                    }
                }
                else
                {
                    spdlog::warn("HookManager: unknown hook type '{}'",
                                 typeStr.toStdString());
                    continue;
                }

                entry.commands.append(cmd);
            }

            m_hooks.append(entry);
        }
    }

    spdlog::info("HookManager: loaded {} hooks from config",
                 static_cast<int>(m_hooks.size()));
}

HookResult HookManager::fireEvent(const HookContext &context) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    HookResult result;

    for (const auto &hook : m_hooks)
    {
        if (!hook.enabled)
            continue;

        if (hook.eventType != context.eventType)
            continue;

        // Check matcher regex against toolName
        if (!hook.matcher.isEmpty())
        {
            QRegularExpression regex(hook.matcher);
            if (!regex.isValid())
            {
                spdlog::warn("HookManager: invalid regex '{}'",
                             hook.matcher.toStdString());
                continue;
            }
            if (!regex.match(context.toolName).hasMatch())
                continue;
        }

        // Execute each command in the hook
        for (const auto &cmd : hook.commands)
        {
            HookResult cmdResult;

            switch (cmd.type)
            {
            case HookCommandType::Shell:
                cmdResult = executeShellCommand(cmd.command, context);
                break;
            case HookCommandType::Prompt:
                cmdResult.proceed = true;
                cmdResult.feedback = cmd.command;
                break;
            case HookCommandType::Http:
                cmdResult = executeHttpCommand(
                    cmd.command, cmd.method, cmd.headers, context);
                break;
            }

            // Aggregate results: any block (proceed=false) overrides
            if (!cmdResult.proceed)
                result.proceed = false;

            if (!cmdResult.feedback.isEmpty())
            {
                if (!result.feedback.isEmpty())
                    result.feedback += QLatin1Char('\n');
                result.feedback += cmdResult.feedback;
            }
        }
    }

    return result;
}

QList<HookEntry> HookManager::listHooks() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hooks;
}

int HookManager::hookCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hooks.size();
}

HookResult HookManager::executeShellCommand(
    const QString &command,
    const HookContext &context) const
{
    HookResult result;
    result.proceed = true;

    spdlog::info("HookManager: executing shell command: {}",
                 command.toStdString());

    QProcess process;
    process.setProgram(QStringLiteral("cmd.exe"));
    process.setArguments({QStringLiteral("/c"), command});

    // Set environment JSON as an env var for the hook
    QString envJson = buildEnvironmentJson(context);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("ACT_HOOK_CONTEXT"), envJson);
    process.setProcessEnvironment(env);

    process.start();
    if (!process.waitForFinished(5000))
    {
        spdlog::warn("HookManager: shell command timed out: {}",
                     command.toStdString());
        process.kill();
        process.waitForFinished(1000);
        result.proceed = false;
        result.feedback = QStringLiteral("Hook timed out: ") + command;
        return result;
    }

    int exitCode = process.exitCode();
    QString stdOut = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    QString stdErr = QString::fromUtf8(process.readAllStandardError()).trimmed();

    if (exitCode != 0)
    {
        spdlog::info("HookManager: shell command exited with code {}: {}",
                     exitCode, command.toStdString());
        result.proceed = false;
        result.feedback = stdOut.isEmpty() ? stdErr : stdOut;
    }
    else
    {
        result.proceed = true;
        result.feedback = stdOut;
    }

    return result;
}

HookResult HookManager::executeHttpCommand(
    const QString &url,
    const QString &method,
    const QMap<QString, QString> &headers,
    const HookContext &context) const
{
    HookResult result;
    result.proceed = true;

    spdlog::info("HookManager: executing HTTP {} to {}",
                 method.toStdString(), url.toStdString());

    // Use curl (available on Windows 10+) to avoid dependency on
    // httplib at the harness layer.
    QStringList args;
    args << QStringLiteral("--silent")
         << QStringLiteral("--show-error")
         << QStringLiteral("--max-time")
         << QStringLiteral("5")
         << QStringLiteral("--write-out")
         << QStringLiteral("\\n%{http_code}");

    QString effectiveMethod = method.isEmpty()
        ? QStringLiteral("POST")
        : method;
    args << QStringLiteral("-X") << effectiveMethod;

    // Add headers
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
    {
        args << QStringLiteral("-H")
             << QStringLiteral("%1: %2").arg(it.key(), it.value());
    }

    // For POST, add JSON body
    if (effectiveMethod.compare(QStringLiteral("GET"),
                                 Qt::CaseInsensitive) != 0)
    {
        QString envJson = buildEnvironmentJson(context);
        args << QStringLiteral("-H")
             << QStringLiteral("Content-Type: application/json")
             << QStringLiteral("-d") << envJson;
    }

    args << url;

    QProcess process;
    process.setProgram(QStringLiteral("curl"));
    process.setArguments(args);
    process.start();

    if (!process.waitForFinished(8000))
    {
        spdlog::warn("HookManager: HTTP request timed out: {}",
                     url.toStdString());
        process.kill();
        process.waitForFinished(1000);
        result.proceed = false;
        result.feedback = QStringLiteral("HTTP hook timed out: ") + url;
        return result;
    }

    int exitCode = process.exitCode();
    if (exitCode != 0)
    {
        QString stdErr = QString::fromUtf8(process.readAllStandardError()).trimmed();
        spdlog::warn("HookManager: curl exited with code {}: {}",
                     exitCode, stdErr.toStdString());
        result.proceed = false;
        result.feedback = stdErr.isEmpty()
            ? QStringLiteral("HTTP hook failed (curl exit code %1)").arg(exitCode)
            : stdErr;
        return result;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());

    // Extract HTTP status code from the last line (write-out output)
    int lastNewline = output.lastIndexOf(QLatin1Char('\n'));
    int httpStatus = 0;
    QString body = output;

    if (lastNewline > 0)
    {
        QString statusStr = output.mid(lastNewline + 1).trimmed();
        bool ok = false;
        httpStatus = statusStr.toInt(&ok);
        if (ok)
        {
            body = output.left(lastNewline);
        }
    }

    if (httpStatus > 0 && (httpStatus < 200 || httpStatus >= 300))
    {
        spdlog::info("HookManager: HTTP {} returned status {}",
                     effectiveMethod.toStdString(), httpStatus);
        result.proceed = false;
    }

    result.feedback = body.trimmed();
    return result;
}

QString HookManager::buildEnvironmentJson(const HookContext &context) const
{
    QJsonObject obj;
    obj[QStringLiteral("eventType")] =
        static_cast<int>(context.eventType);
    obj[QStringLiteral("toolName")] = context.toolName;
    obj[QStringLiteral("toolParams")] = context.toolParams;
    obj[QStringLiteral("toolSuccess")] = context.toolSuccess;
    obj[QStringLiteral("toolOutput")] = context.toolOutput;
    obj[QStringLiteral("userInput")] = context.userInput;
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace act::harness
