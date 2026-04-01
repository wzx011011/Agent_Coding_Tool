#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include "core/error_codes.h"
#include "harness/task_manager.h"
#include "harness/tools/task_create_tool.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

class TaskCreateToolTest : public ::testing::Test
{
protected:
    TaskManager manager;
    TaskCreateTool tool{manager};
};

// --- Name, description, permissions ---

TEST_F(TaskCreateToolTest, NameAndDescription)
{
    EXPECT_EQ(tool.name(), QStringLiteral("task_create"));
    EXPECT_FALSE(tool.description().isEmpty());
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Write);
    EXPECT_FALSE(tool.isThreadSafe());
}

// --- Schema ---

TEST_F(TaskCreateToolTest, SchemaRequiresSubject)
{
    auto schema = tool.schema();
    auto required = schema.value(QStringLiteral("required")).toArray();
    ASSERT_GE(required.size(), 1);
    EXPECT_EQ(required.at(0).toString(), QStringLiteral("subject"));

    auto props = schema.value(QStringLiteral("properties")).toObject();
    EXPECT_TRUE(props.contains(QStringLiteral("subject")));
    EXPECT_TRUE(props.contains(QStringLiteral("description")));
    EXPECT_TRUE(props.contains(QStringLiteral("activeForm")));
}

// --- Execute with valid params ---

TEST_F(TaskCreateToolTest, ExecuteWithSubjectOnly)
{
    QJsonObject params;
    params[QStringLiteral("subject")] = QStringLiteral("Write tests");

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.output.indexOf(QStringLiteral("Write tests")), -1);
    EXPECT_EQ(result.errorCode, QString());
}

TEST_F(TaskCreateToolTest, ExecuteReturnsTaskIdInMetadata)
{
    QJsonObject params;
    params[QStringLiteral("subject")] = QStringLiteral("My task");

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.metadata.value(QStringLiteral("taskId")).toString().isEmpty());
    EXPECT_EQ(result.metadata.value(QStringLiteral("subject")).toString(),
              QStringLiteral("My task"));
}

TEST_F(TaskCreateToolTest, ExecuteWithDescriptionAndActiveForm)
{
    QJsonObject params;
    params[QStringLiteral("subject")] = QStringLiteral("Build");
    params[QStringLiteral("description")] = QStringLiteral("Build the project");
    params[QStringLiteral("activeForm")] = QStringLiteral("Building project");

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);

    // Verify the task was stored correctly
    auto taskId = result.metadata.value(QStringLiteral("taskId")).toString();
    auto task = manager.getTask(taskId);
    EXPECT_EQ(task.subject, QStringLiteral("Build"));
    EXPECT_EQ(task.description, QStringLiteral("Build the project"));
    EXPECT_EQ(task.activeForm, QStringLiteral("Building project"));
}

TEST_F(TaskCreateToolTest, ExecuteCreatesMultipleTasks)
{
    QJsonObject params1;
    params1[QStringLiteral("subject")] = QStringLiteral("Task A");
    auto r1 = tool.execute(params1);

    QJsonObject params2;
    params2[QStringLiteral("subject")] = QStringLiteral("Task B");
    auto r2 = tool.execute(params2);

    EXPECT_TRUE(r1.success);
    EXPECT_TRUE(r2.success);

    auto id1 = r1.metadata.value(QStringLiteral("taskId")).toString();
    auto id2 = r2.metadata.value(QStringLiteral("taskId")).toString();
    EXPECT_NE(id1, id2);

    auto tasks = manager.listTasks();
    EXPECT_EQ(tasks.size(), 2);
}

// --- Execute with invalid params ---

TEST_F(TaskCreateToolTest, ExecuteMissingSubject)
{
    QJsonObject params;

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(TaskCreateToolTest, ExecuteEmptySubject)
{
    QJsonObject params;
    params[QStringLiteral("subject")] = QStringLiteral("");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}
