#include "framework/resume_manager.h"

#include <QJsonArray>
#include <spdlog/spdlog.h>

#include "core/types.h"

namespace act::framework
{

void ResumeManager::saveCheckpoint(const QString &taskId,
                                    const AgentLoop::Checkpoint &cp)
{
    m_checkpoints[taskId] = cp;
    spdlog::info("ResumeManager: saved checkpoint for task '{}' "
                 "({} messages, {} turns)",
                 taskId.toStdString(),
                 cp.messages.size(),
                 cp.turnCount);
}

std::optional<AgentLoop::Checkpoint> ResumeManager::loadCheckpoint(
    const QString &taskId) const
{
    auto it = m_checkpoints.find(taskId);
    if (it != m_checkpoints.end())
        return it.value();
    return std::nullopt;
}

bool ResumeManager::hasCheckpoint(const QString &taskId) const
{
    return m_checkpoints.contains(taskId);
}

void ResumeManager::removeCheckpoint(const QString &taskId)
{
    if (m_checkpoints.remove(taskId) > 0)
    {
        spdlog::info("ResumeManager: removed checkpoint for task '{}'",
                     taskId.toStdString());
    }
}

void ResumeManager::clearAll()
{
    m_checkpoints.clear();
    spdlog::info("ResumeManager: cleared all checkpoints");
}

QStringList ResumeManager::savedTaskIds() const
{
    return m_checkpoints.keys();
}

QJsonObject ResumeManager::checkpointToJson(
    const AgentLoop::Checkpoint &cp)
{
    QJsonObject obj;
    obj[QStringLiteral("state")] =
        static_cast<int>(cp.state);

    QJsonArray msgsArr;
    for (const auto &msg : cp.messages)
    {
        QJsonObject msgObj;
        msgObj[QStringLiteral("role")] =
            static_cast<int>(msg.role);
        msgObj[QStringLiteral("content")] = msg.content;
        msgObj[QStringLiteral("tool_call_id")] = msg.toolCallId;

        if (!msg.toolCall.id.isEmpty())
        {
            QJsonObject tc;
            tc[QStringLiteral("id")] = msg.toolCall.id;
            tc[QStringLiteral("name")] = msg.toolCall.name;
            tc[QStringLiteral("params")] = msg.toolCall.params;
            msgObj[QStringLiteral("tool_call")] = tc;
        }

        msgsArr.append(msgObj);
    }
    obj[QStringLiteral("messages")] = msgsArr;
    obj[QStringLiteral("turn_count")] = cp.turnCount;

    return obj;
}

std::optional<AgentLoop::Checkpoint> ResumeManager::checkpointFromJson(
    const QJsonObject &json)
{
    AgentLoop::Checkpoint cp;

    auto stateVal = json.value(QStringLiteral("state"));
    if (!stateVal.isDouble())
        return std::nullopt;
    cp.state = static_cast<act::core::TaskState>(
        stateVal.toInt(static_cast<int>(act::core::TaskState::Idle)));

    cp.turnCount = json.value(QStringLiteral("turn_count")).toInt(0);

    auto msgsVal = json.value(QStringLiteral("messages"));
    if (!msgsVal.isArray())
        return std::nullopt;

    const auto msgsArr = msgsVal.toArray();
    for (const auto &msgVal : msgsArr)
    {
        if (!msgVal.isObject())
            continue;

        const auto msgObj = msgVal.toObject();
        act::core::LLMMessage msg;
        msg.role = static_cast<act::core::MessageRole>(
            msgObj.value(QStringLiteral("role")).toInt(0));
        msg.content = msgObj.value(QStringLiteral("content")).toString();
        msg.toolCallId = msgObj.value(QStringLiteral("tool_call_id")).toString();

        auto tcVal = msgObj.value(QStringLiteral("tool_call"));
        if (tcVal.isObject())
        {
            const auto tc = tcVal.toObject();
            msg.toolCall.id = tc.value(QStringLiteral("id")).toString();
            msg.toolCall.name = tc.value(QStringLiteral("name")).toString();
            msg.toolCall.params = tc.value(QStringLiteral("params")).toObject();
        }

        cp.messages.append(msg);
    }

    return cp;
}

QJsonObject ResumeManager::serialize() const
{
    QJsonObject root;
    for (auto it = m_checkpoints.constBegin();
         it != m_checkpoints.constEnd(); ++it)
    {
        root[it.key()] = checkpointToJson(it.value());
    }
    return root;
}

void ResumeManager::deserialize(const QJsonObject &json)
{
    for (auto it = json.constBegin(); it != json.constEnd(); ++it)
    {
        if (it.value().isObject())
        {
            auto cp = checkpointFromJson(it.value().toObject());
            if (cp)
                m_checkpoints[it.key()] = *cp;
        }
    }
    spdlog::info("ResumeManager: deserialized {} checkpoints",
                 m_checkpoints.size());
}

} // namespace act::framework
