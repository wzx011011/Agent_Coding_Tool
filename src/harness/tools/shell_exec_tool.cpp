#include "harness/tools/shell_exec_tool.h"

#include "core/error_codes.h"
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <spdlog/spdlog.h>

namespace act::harness {

ShellExecTool::ShellExecTool(act::infrastructure::IProcess &proc, QString workspaceRoot)
    : m_proc(proc), m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot))) {
    // Default deny list for dangerous commands
    m_denylist = {
        QStringLiteral("rm -rf /"), QStringLiteral("rm -rf /*"),       QStringLiteral(":(){ :|:& };:"),
        QStringLiteral("mkfs"),     QStringLiteral("dd if=/dev/zero"), QStringLiteral("chmod -R 777 /"),
        QStringLiteral("shutdown"), QStringLiteral("reboot"),          QStringLiteral("halt"),
        QStringLiteral("init 0"),   QStringLiteral("> /dev/sda"),
    };
}

QString ShellExecTool::name() const {
    return QStringLiteral("shell_exec");
}

QString ShellExecTool::description() const {
    return QStringLiteral("Execute a shell command and return the output. "
                          "Supports timeout and command filtering.");
}

QJsonObject ShellExecTool::schema() const {
    QJsonObject props;

    props[QStringLiteral("command")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] = QStringLiteral("The shell command to execute");
        return obj;
    }();

    props[QStringLiteral("args")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("array");
        obj[QStringLiteral("items")] = QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}};
        obj[QStringLiteral("description")] = QStringLiteral("Optional arguments for the command");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{QStringLiteral("command")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult ShellExecTool::execute(const QJsonObject &params) {
    // Debug: log received params
    spdlog::debug("ShellExecTool params: {}", QJsonDocument(params).toJson().toStdString());

    auto cmdValue = params.value(QStringLiteral("command"));
    if (!cmdValue.isString() || cmdValue.toString().isEmpty()) {
        spdlog::warn("ShellExecTool: 'command' param missing or invalid. Keys: {}", [params]() {
            QStringList keys;
            for (auto it = params.begin(); it != params.end(); ++it)
                keys.append(it.key());
            return keys.join(", ").toStdString();
        }());
        return act::core::ToolResult::err(act::core::errors::INVALID_PARAMS,
                                          QStringLiteral("'command' must be a non-empty string"));
    }

    const QString command = cmdValue.toString().trimmed();

    // Security checks
    if (isCommandBlocked(command)) {
        return act::core::ToolResult::err(act::core::errors::COMMAND_BLOCKED,
                                          QStringLiteral("Command is blocked: %1").arg(command));
    }

    if (!isCommandAllowed(command)) {
        return act::core::ToolResult::err(act::core::errors::COMMAND_BLOCKED,
                                          QStringLiteral("Command not in allowlist: %1").arg(command));
    }

    // Extract args if provided
    QStringList args;
    auto argsValue = params.value(QStringLiteral("args"));
    if (argsValue.isArray()) {
        const auto arr = argsValue.toArray();
        for (const auto &item : arr)
            args.append(item.toString());
    }

    QString output;
    int exitCode = -1;

    // Execute synchronously (IProcess::execute is synchronous for LocalProcess)
    m_proc.execute(
        command, args,
        [&output, &exitCode](int code, QString out) {
            exitCode = code;
            output = std::move(out);
        },
        m_timeoutMs);

    QJsonObject metadata;
    metadata[QStringLiteral("exitCode")] = exitCode;

    if (exitCode == -1) {
        return act::core::ToolResult::err(act::core::errors::TIMEOUT,
                                          QStringLiteral("Command timed out: %1").arg(output.left(500)), metadata);
    }

    if (exitCode != 0) {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("Command failed with exit code %1: %2").arg(exitCode).arg(output.left(500)), metadata);
    }

    return act::core::ToolResult::ok(output, metadata);
}

act::core::PermissionLevel ShellExecTool::permissionLevel() const {
    return act::core::PermissionLevel::Exec;
}

bool ShellExecTool::isThreadSafe() const {
    return false;
}

void ShellExecTool::setTimeoutMs(int ms) {
    m_timeoutMs = ms;
}

void ShellExecTool::addToAllowlist(const QString &command) {
    if (!m_allowlist.contains(command))
        m_allowlist.append(command);
}

void ShellExecTool::addToDenylist(const QString &command) {
    if (!m_denylist.contains(command))
        m_denylist.append(command);
}

bool ShellExecTool::isCommandBlocked(const QString &command) const {
    for (const auto &blocked : m_denylist) {
        if (command.contains(blocked))
            return true;
    }
    return false;
}

bool ShellExecTool::isCommandAllowed(const QString &command) const {
    if (m_allowlist.isEmpty())
        return true; // No allowlist means all allowed

    for (const auto &allowed : m_allowlist) {
        if (command.startsWith(allowed))
            return true;
    }
    return false;
}

} // namespace act::harness
