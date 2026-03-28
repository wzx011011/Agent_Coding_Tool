#include "harness/tools/git_branch_tool.h"

#include <QDir>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

GitBranchTool::GitBranchTool(act::infrastructure::IProcess &proc,
                             QString workspaceRoot)
    : m_proc(proc)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString GitBranchTool::name() const
{
    return QStringLiteral("git_branch");
}

QString GitBranchTool::description() const
{
    return QStringLiteral(
        "Manage git branches. Actions: list (default), create, "
        "delete, switch. List supports 'current' flag to show only "
        "the current branch.");
}

QJsonObject GitBranchTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("action")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("enum")] = QJsonArray{
            QStringLiteral("list"),
            QStringLiteral("create"),
            QStringLiteral("delete"),
            QStringLiteral("switch"),
        };
        obj[QStringLiteral("description")] =
            QStringLiteral("Branch action: list, create, delete, or switch");
        return obj;
    }();

    props[QStringLiteral("name")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("string");
        obj[QStringLiteral("description")] =
            QStringLiteral("Branch name (required for create, delete, "
                           "switch)");
        return obj;
    }();

    props[QStringLiteral("switch_to")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("boolean");
        obj[QStringLiteral("description")] =
            QStringLiteral("When creating, also switch to the new branch "
                           "(default: false)");
        return obj;
    }();

    props[QStringLiteral("force")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("boolean");
        obj[QStringLiteral("description")] =
            QStringLiteral("Force delete (default: false)");
        return obj;
    }();

    props[QStringLiteral("current")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("boolean");
        obj[QStringLiteral("description")] =
            QStringLiteral("Show only current branch name (list action, "
                           "default: false)");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("action")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult GitBranchTool::execute(const QJsonObject &params)
{
    auto action = params.value(QStringLiteral("action")).toString();
    if (action.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("git_branch requires 'action' parameter "
                           "(list, create, delete, switch)"));
    }

    if (action == QStringLiteral("list"))
    {
        bool showCurrent =
            params.value(QStringLiteral("current")).toBool(false);
        return listBranches(showCurrent);
    }

    if (action == QStringLiteral("create"))
    {
        auto name = params.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
        {
            return act::core::ToolResult::err(
                act::core::errors::INVALID_PARAMS,
                QStringLiteral("git_branch create requires 'name' "
                               "parameter"));
        }
        bool switchTo =
            params.value(QStringLiteral("switch_to")).toBool(false);
        return createBranch(name, switchTo);
    }

    if (action == QStringLiteral("delete"))
    {
        auto name = params.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
        {
            return act::core::ToolResult::err(
                act::core::errors::INVALID_PARAMS,
                QStringLiteral("git_branch delete requires 'name' "
                               "parameter"));
        }
        bool force = params.value(QStringLiteral("force")).toBool(false);
        return deleteBranch(name, force);
    }

    if (action == QStringLiteral("switch"))
    {
        auto name = params.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
        {
            return act::core::ToolResult::err(
                act::core::errors::INVALID_PARAMS,
                QStringLiteral("git_branch switch requires 'name' "
                               "parameter"));
        }
        return switchBranch(name);
    }

    return act::core::ToolResult::err(
        act::core::errors::INVALID_PARAMS,
        QStringLiteral("Unknown action: '%1'. Valid: list, create, "
                       "delete, switch")
            .arg(action));
}

act::core::ToolResult GitBranchTool::listBranches(bool showCurrent)
{
    QStringList args;
    if (showCurrent)
    {
        args << QStringLiteral("branch") << QStringLiteral("--show-current");
    }
    else
    {
        args << QStringLiteral("branch");
    }

    QString output;
    int exitCode = -1;

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

    return act::core::ToolResult::ok(output.trimmed());
}

act::core::ToolResult GitBranchTool::createBranch(const QString &name,
                                                   bool switchTo)
{
    QStringList args;
    if (switchTo)
    {
        args << QStringLiteral("switch") << QStringLiteral("-c") << name;
    }
    else
    {
        args << QStringLiteral("branch") << name;
    }

    QString output;
    int exitCode = -1;

    m_proc.execute(QStringLiteral("git"), args,
        [&output, &exitCode](int code, QString out) {
            exitCode = code;
            output = std::move(out);
        });

    if (exitCode != 0)
    {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("Failed to create branch '%1': %2")
                .arg(name, output));
    }

    return act::core::ToolResult::ok(
        QStringLiteral("Branch '%1' created%2")
            .arg(name,
                 switchTo ? QStringLiteral(" and switched to")
                          : QString()));
}

act::core::ToolResult GitBranchTool::deleteBranch(const QString &name,
                                                   bool force)
{
    QStringList args;
    args << QStringLiteral("branch");
    if (force)
        args << QStringLiteral("-D");
    else
        args << QStringLiteral("-d");
    args << name;

    QString output;
    int exitCode = -1;

    m_proc.execute(QStringLiteral("git"), args,
        [&output, &exitCode](int code, QString out) {
            exitCode = code;
            output = std::move(out);
        });

    if (exitCode != 0)
    {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("Failed to delete branch '%1': %2")
                .arg(name, output));
    }

    return act::core::ToolResult::ok(
        QStringLiteral("Branch '%1' deleted").arg(name));
}

act::core::ToolResult GitBranchTool::switchBranch(const QString &name)
{
    QStringList args;
    args << QStringLiteral("switch") << name;

    QString output;
    int exitCode = -1;

    m_proc.execute(QStringLiteral("git"), args,
        [&output, &exitCode](int code, QString out) {
            exitCode = code;
            output = std::move(out);
        });

    if (exitCode != 0)
    {
        return act::core::ToolResult::err(
            act::core::errors::COMMAND_FAILED,
            QStringLiteral("Failed to switch to branch '%1': %2")
                .arg(name, output));
    }

    return act::core::ToolResult::ok(
        QStringLiteral("Switched to branch '%1'").arg(name));
}

act::core::PermissionLevel GitBranchTool::permissionLevel() const
{
    return act::core::PermissionLevel::Exec;
}

bool GitBranchTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
