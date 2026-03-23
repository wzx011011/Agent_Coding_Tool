#include "framework/task_graph.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <queue>

namespace act::framework
{

// ============================================================
// TaskGraph
// ============================================================

bool TaskGraph::addTask(const TaskNode &task)
{
    if (findTask(task.id) != nullptr)
        return false;
    m_tasks.append(task);
    return true;
}

bool TaskGraph::updateState(const QString &id, TaskNodeState state,
                              const QString &result)
{
    auto *task = findTaskMutable(id);
    if (!task)
        return false;
    task->state = state;
    if (!result.isEmpty())
        task->result = result;
    return true;
}

const TaskNode *TaskGraph::findTask(const QString &id) const
{
    for (const auto &task : m_tasks)
    {
        if (task.id == id)
            return &task;
    }
    return nullptr;
}

TaskNode *TaskGraph::findTaskMutable(const QString &id)
{
    for (auto &task : m_tasks)
    {
        if (task.id == id)
            return &task;
    }
    return nullptr;
}

QStringList TaskGraph::listTasks() const
{
    QStringList ids;
    for (const auto &task : m_tasks)
        ids.append(task.id);
    return ids;
}

bool TaskGraph::computeWaves(TaskWave &waves) const
{
    if (hasCycle())
        return false;

    waves.clear();

    // Compute in-degrees
    QMap<QString, int> inDeg;
    QMap<QString, QStringList> adj; // id -> list of dependents
    for (const auto &task : m_tasks)
    {
        inDeg[task.id] = 0;
        adj[task.id] = {};
    }
    for (const auto &task : m_tasks)
    {
        for (const auto &dep : task.dependencies)
        {
            if (inDeg.contains(dep))
            {
                adj[dep].append(task.id);
                ++inDeg[task.id];
            }
        }
    }

    // Kahn's algorithm
    std::queue<QString> queue;
    for (auto it = inDeg.constBegin(); it != inDeg.constEnd(); ++it)
    {
        if (it.value() == 0)
            queue.push(it.key());
    }

    while (!queue.empty())
    {
        QStringList wave;
        int levelSize = static_cast<int>(queue.size());
        for (int i = 0; i < levelSize; ++i)
        {
            QString current = queue.front();
            queue.pop();
            wave.append(current);

            for (const auto &next : adj[current])
            {
                --inDeg[next];
                if (inDeg[next] == 0)
                    queue.push(next);
            }
        }
        if (!wave.isEmpty())
            waves.append(wave);
    }

    return !waves.isEmpty();
}

QStringList TaskGraph::readyTasks() const
{
    QStringList ready;
    for (const auto &task : m_tasks)
    {
        if (task.state != TaskNodeState::Pending)
            continue;

        bool allDepsCompleted = true;
        for (const auto &depId : task.dependencies)
        {
            auto *dep = findTask(depId);
            if (!dep || dep->state != TaskNodeState::Completed)
            {
                allDepsCompleted = false;
                break;
            }
        }

        if (allDepsCompleted)
            ready.append(task.id);
    }
    return ready;
}

bool TaskGraph::allCompleted() const
{
    for (const auto &task : m_tasks)
    {
        if (task.state != TaskNodeState::Completed &&
            task.state != TaskNodeState::Skipped)
            return false;
    }
    return !m_tasks.isEmpty();
}

int TaskGraph::size() const
{
    return m_tasks.size();
}

QStringList TaskGraph::tasksByState(TaskNodeState state) const
{
    QStringList result;
    for (const auto &task : m_tasks)
    {
        if (task.state == state)
            result.append(task.id);
    }
    return result;
}

bool TaskGraph::hasCycle() const
{
    QMap<QString, int> visited; // 0=unvisited, 1=in-progress, 2=done

    std::function<bool(const QString &)> dfs = [&](const QString &id) -> bool {
        visited[id] = 1;
        auto *task = findTask(id);
        if (!task)
            return false;
        for (const auto &dep : task->dependencies)
        {
            if (visited.value(dep) == 1)
                return true;
            if (visited.value(dep) == 0 && dfs(dep))
                return true;
        }
        visited[id] = 2;
        return false;
    };

    for (const auto &task : m_tasks)
    {
        if (visited.value(task.id) == 0)
        {
            if (dfs(task.id))
                return true;
        }
    }
    return false;
}

int TaskGraph::inDegree(const QString &id) const
{
    auto *task = findTask(id);
    if (!task)
        return 0;
    return task->dependencies.size();
}

// ============================================================
// TaskStateStore
// ============================================================

static QString stateToString(TaskNodeState state)
{
    switch (state)
    {
    case TaskNodeState::Pending:   return QStringLiteral("pending");
    case TaskNodeState::Running:   return QStringLiteral("running");
    case TaskNodeState::Completed: return QStringLiteral("completed");
    case TaskNodeState::Failed:    return QStringLiteral("failed");
    case TaskNodeState::Skipped:   return QStringLiteral("skipped");
    }
    return QStringLiteral("unknown");
}

static TaskNodeState stringToState(const QString &str)
{
    if (str == QLatin1String("running"))   return TaskNodeState::Running;
    if (str == QLatin1String("completed")) return TaskNodeState::Completed;
    if (str == QLatin1String("failed"))    return TaskNodeState::Failed;
    if (str == QLatin1String("skipped"))   return TaskNodeState::Skipped;
    return TaskNodeState::Pending;
}

QJsonObject TaskStateStore::serialize(const TaskGraph &graph) const
{
    QJsonObject root;
    QJsonArray tasksArr;

    auto allTasks = graph.listTasks();
    for (const auto &id : allTasks)
    {
        auto *task = graph.findTask(id);
        if (!task)
            continue;

        QJsonObject obj;
        obj[QStringLiteral("id")] = task->id;
        obj[QStringLiteral("title")] = task->title;
        obj[QStringLiteral("description")] = task->description;
        obj[QStringLiteral("state")] = stateToString(task->state);
        obj[QStringLiteral("result")] = task->result;
        obj[QStringLiteral("error")] = task->error;
        obj[QStringLiteral("metadata")] = task->metadata;

        QJsonArray depsArr;
        for (const auto &dep : task->dependencies)
            depsArr.append(dep);
        obj[QStringLiteral("dependencies")] = depsArr;

        tasksArr.append(obj);
    }

    root[QStringLiteral("tasks")] = tasksArr;
    return root;
}

TaskGraph TaskStateStore::deserialize(const QJsonObject &json) const
{
    TaskGraph graph;
    auto tasksArr = json.value(QStringLiteral("tasks")).toArray();

    for (const auto &item : tasksArr)
    {
        auto obj = item.toObject();
        TaskNode node;
        node.id = obj.value(QStringLiteral("id")).toString();
        node.title = obj.value(QStringLiteral("title")).toString();
        node.description = obj.value(QStringLiteral("description")).toString();
        node.state = stringToState(obj.value(QStringLiteral("state")).toString());
        node.result = obj.value(QStringLiteral("result")).toString();
        node.error = obj.value(QStringLiteral("error")).toString();
        node.metadata = obj.value(QStringLiteral("metadata")).toObject();

        auto depsArr = obj.value(QStringLiteral("dependencies")).toArray();
        for (const auto &dep : depsArr)
            node.dependencies.append(dep.toString());

        graph.addTask(node);
    }

    return graph;
}

bool TaskStateStore::saveToFile(const TaskGraph &graph,
                                  const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QJsonDocument doc(serialize(graph));
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

TaskGraph TaskStateStore::loadFromFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};

    return deserialize(doc.object());
}

} // namespace act::framework
