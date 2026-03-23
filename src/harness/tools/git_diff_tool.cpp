#include "harness/tools/git_diff_tool.h"

#include <QDir>
#include <QJsonArray>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

GitDiffTool::GitDiffTool(act::infrastructure::IProcess &proc,
                           QString workspaceRoot)
    : m_proc(proc)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString GitDiffTool::name() const
{
    return QStringLiteral("git_diff");
}

QString GitDiffTool::description() const
{
    return QStringLiteral("Show git diff. Optionally pass 'paths' array to "
                          "diff specific files, or 'staged' to show staged changes.");
}

QJsonObject GitDiffTool::schema() const
{
    QJsonObject props;

    props[QStringLiteral("paths")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("array");
        obj[QStringLiteral("items")] = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")}};
        obj[QStringLiteral("description")] =
            QStringLiteral("Optional file paths to diff");
        return obj;
    }();

    props[QStringLiteral("staged")] = [] {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("boolean");
        obj[QStringLiteral("description")] =
            QStringLiteral("Show staged changes (--cached)");
        return obj;
    }();

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] = QJsonArray{};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult GitDiffTool::execute(const QJsonObject &params)
{
    QStringList args;
    args << QStringLiteral("diff");

    bool staged = params.value(QStringLiteral("staged")).toBool(false);
    if (staged)
        args << QStringLiteral("--cached");

    auto pathsValue = params.value(QStringLiteral("paths"));
    if (pathsValue.isArray())
    {
        const auto arr = pathsValue.toArray();
        if (!arr.isEmpty())
        {
            args << QStringLiteral("--");
            for (const auto &item : arr)
                args << item.toString();
        }
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
            QStringLiteral("git diff failed in: %1").arg(m_workspaceRoot));
    }

    return act::core::ToolResult::ok(output);
}

act::core::PermissionLevel GitDiffTool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool GitDiffTool::isThreadSafe() const
{
    return false;
}

} // namespace act::harness
