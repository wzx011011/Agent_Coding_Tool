#include "harness/tools/git_status_tool.h"

#include <QDir>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

GitStatusTool::GitStatusTool(act::infrastructure::IProcess &proc,
                               QString workspaceRoot)
    : m_proc(proc)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString GitStatusTool::name() const
{
    return QStringLiteral("git_status");
}

QString GitStatusTool::description() const
{
    return QStringLiteral("Show working tree status using git status --porcelain.");
}

QJsonObject GitStatusTool::schema() const
{
    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{};
    schema[QStringLiteral("properties")] = QJsonObject{};
    return schema;
}

act::core::ToolResult GitStatusTool::execute(const QJsonObject & /*params*/)
{
    QString output;
    int exitCode = -1;

    QStringList args;
    args << QStringLiteral("status") << QStringLiteral("--porcelain");

    m_proc.execute(QStringLiteral("git"), args,
        [&output, &exitCode](int code, QString out) {
            exitCode = code;
            output = std::move(out);
        });

    if (exitCode != 0)
    {
        return act::core::ToolResult::err(
            act::core::errors::NOT_GIT_REPO,
            QStringLiteral("Not a git repository: %1").arg(m_workspaceRoot));
    }

    return act::core::ToolResult::ok(output);
}

act::core::PermissionLevel GitStatusTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool GitStatusTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
