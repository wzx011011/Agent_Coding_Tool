#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include "core/error_codes.h"
#include "harness/tools/todo_write_tool.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

class TodoWriteToolTest : public ::testing::Test
{
protected:
    TodoWriteTool tool;
};

// --- Name, description, permissions ---

TEST_F(TodoWriteToolTest, NameAndDescription)
{
    EXPECT_EQ(tool.name(), QStringLiteral("todo_write"));
    EXPECT_FALSE(tool.description().isEmpty());
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);
    EXPECT_FALSE(tool.isThreadSafe());
}

// --- Schema ---

TEST_F(TodoWriteToolTest, SchemaRequiresAction)
{
    auto schema = tool.schema();
    auto required = schema.value(QStringLiteral("required")).toArray();
    ASSERT_GE(required.size(), 1);
    EXPECT_EQ(required.at(0).toString(), QStringLiteral("action"));

    auto props = schema.value(QStringLiteral("properties")).toObject();
    EXPECT_TRUE(props.contains(QStringLiteral("action")));
    EXPECT_TRUE(props.contains(QStringLiteral("subject")));
    EXPECT_TRUE(props.contains(QStringLiteral("description")));
    EXPECT_TRUE(props.contains(QStringLiteral("status")));
}

// --- Add action ---

TEST_F(TodoWriteToolTest, AddItemSucceeds)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("add");
    params[QStringLiteral("subject")] = QStringLiteral("Buy milk");

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.output.indexOf(QStringLiteral("Buy milk")), -1);
    EXPECT_EQ(tool.count(), 1);
    EXPECT_EQ(tool.items().first().subject, QStringLiteral("Buy milk"));
    EXPECT_EQ(tool.items().first().status, TodoWriteTool::Status::Pending);
}

TEST_F(TodoWriteToolTest, AddItemWithDescription)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("add");
    params[QStringLiteral("subject")] = QStringLiteral("Write tests");
    params[QStringLiteral("description")] = QStringLiteral("Unit tests for new tools");

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(tool.items().first().description,
              QStringLiteral("Unit tests for new tools"));
}

TEST_F(TodoWriteToolTest, AddItemWithStatus)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("add");
    params[QStringLiteral("subject")] = QStringLiteral("In-progress task");
    params[QStringLiteral("status")] = QStringLiteral("in_progress");

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(tool.items().first().status,
              TodoWriteTool::Status::InProgress);
}

TEST_F(TodoWriteToolTest, AddMultipleItems)
{
    for (int i = 0; i < 5; ++i)
    {
        QJsonObject params;
        params[QStringLiteral("action")] = QStringLiteral("add");
        params[QStringLiteral("subject")] = QStringLiteral("Task %1").arg(i);
        tool.execute(params);
    }
    EXPECT_EQ(tool.count(), 5);
}

TEST_F(TodoWriteToolTest, AddMissingSubject)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("add");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

// --- Remove action ---

TEST_F(TodoWriteToolTest, RemoveItemSucceeds)
{
    QJsonObject addParams;
    addParams[QStringLiteral("action")] = QStringLiteral("add");
    addParams[QStringLiteral("subject")] = QStringLiteral("Remove me");
    tool.execute(addParams);
    ASSERT_EQ(tool.count(), 1);

    QJsonObject removeParams;
    removeParams[QStringLiteral("action")] = QStringLiteral("remove");
    removeParams[QStringLiteral("subject")] = QStringLiteral("Remove me");

    auto result = tool.execute(removeParams);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(tool.count(), 0);
}

TEST_F(TodoWriteToolTest, RemoveNotFound)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("remove");
    params[QStringLiteral("subject")] = QStringLiteral("Nonexistent");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, STRING_NOT_FOUND);
}

TEST_F(TodoWriteToolTest, RemoveMissingSubject)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("remove");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

// --- Complete action ---

TEST_F(TodoWriteToolTest, CompleteItemSucceeds)
{
    QJsonObject addParams;
    addParams[QStringLiteral("action")] = QStringLiteral("add");
    addParams[QStringLiteral("subject")] = QStringLiteral("Do homework");
    tool.execute(addParams);

    QJsonObject completeParams;
    completeParams[QStringLiteral("action")] = QStringLiteral("complete");
    completeParams[QStringLiteral("subject")] = QStringLiteral("Do homework");

    auto result = tool.execute(completeParams);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(tool.items().first().status, TodoWriteTool::Status::Completed);
}

TEST_F(TodoWriteToolTest, CompleteNotFound)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("complete");
    params[QStringLiteral("subject")] = QStringLiteral("Ghost task");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, STRING_NOT_FOUND);
}

TEST_F(TodoWriteToolTest, CompleteMissingSubject)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("complete");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

// --- List action ---

TEST_F(TodoWriteToolTest, ListEmpty)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("list");

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.output.indexOf(QStringLiteral("empty")), -1);
    EXPECT_EQ(result.metadata.value(QStringLiteral("count")).toInt(), 0);
}

TEST_F(TodoWriteToolTest, ListWithItems)
{
    QJsonObject addParams;
    addParams[QStringLiteral("action")] = QStringLiteral("add");
    addParams[QStringLiteral("subject")] = QStringLiteral("Task A");
    tool.execute(addParams);
    addParams[QStringLiteral("subject")] = QStringLiteral("Task B");
    tool.execute(addParams);

    QJsonObject listParams;
    listParams[QStringLiteral("action")] = QStringLiteral("list");

    auto result = tool.execute(listParams);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.output.indexOf(QStringLiteral("Task A")), -1);
    EXPECT_NE(result.output.indexOf(QStringLiteral("Task B")), -1);
    EXPECT_EQ(result.metadata.value(QStringLiteral("count")).toInt(), 2);
    EXPECT_EQ(result.metadata.value(QStringLiteral("pending")).toInt(), 2);
    EXPECT_EQ(result.metadata.value(QStringLiteral("completed")).toInt(), 0);
}

TEST_F(TodoWriteToolTest, ListShowsCompletedMarker)
{
    QJsonObject addParams;
    addParams[QStringLiteral("action")] = QStringLiteral("add");
    addParams[QStringLiteral("subject")] = QStringLiteral("Done task");
    tool.execute(addParams);

    QJsonObject completeParams;
    completeParams[QStringLiteral("action")] = QStringLiteral("complete");
    completeParams[QStringLiteral("subject")] = QStringLiteral("Done task");
    tool.execute(completeParams);

    QJsonObject listParams;
    listParams[QStringLiteral("action")] = QStringLiteral("list");

    auto result = tool.execute(listParams);
    EXPECT_NE(result.output.indexOf(QStringLiteral("[x]")), -1);
    EXPECT_EQ(result.metadata.value(QStringLiteral("completed")).toInt(), 1);
}

// --- Clear action ---

TEST_F(TodoWriteToolTest, ClearSucceeds)
{
    QJsonObject addParams;
    addParams[QStringLiteral("action")] = QStringLiteral("add");
    addParams[QStringLiteral("subject")] = QStringLiteral("Clear me");
    tool.execute(addParams);
    ASSERT_EQ(tool.count(), 1);

    QJsonObject clearParams;
    clearParams[QStringLiteral("action")] = QStringLiteral("clear");

    auto result = tool.execute(clearParams);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(tool.count(), 0);
}

TEST_F(TodoWriteToolTest, ClearEmptyList)
{
    QJsonObject clearParams;
    clearParams[QStringLiteral("action")] = QStringLiteral("clear");

    auto result = tool.execute(clearParams);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(tool.count(), 0);
}

// --- Invalid action ---

TEST_F(TodoWriteToolTest, InvalidAction)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("explode");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(TodoWriteToolTest, MissingAction)
{
    QJsonObject params;

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

// --- Status parsing ---

TEST_F(TodoWriteToolTest, ParseStatusValues)
{
    EXPECT_EQ(TodoWriteTool::parseStatus(QStringLiteral("pending")),
              TodoWriteTool::Status::Pending);
    EXPECT_EQ(TodoWriteTool::parseStatus(QStringLiteral("in_progress")),
              TodoWriteTool::Status::InProgress);
    EXPECT_EQ(TodoWriteTool::parseStatus(QStringLiteral("completed")),
              TodoWriteTool::Status::Completed);
    EXPECT_EQ(TodoWriteTool::parseStatus(QStringLiteral("invalid")),
              std::nullopt);
}

TEST_F(TodoWriteToolTest, StatusToStringValues)
{
    EXPECT_EQ(TodoWriteTool::statusToString(TodoWriteTool::Status::Pending),
              QStringLiteral("pending"));
    EXPECT_EQ(TodoWriteTool::statusToString(TodoWriteTool::Status::InProgress),
              QStringLiteral("in_progress"));
    EXPECT_EQ(TodoWriteTool::statusToString(TodoWriteTool::Status::Completed),
              QStringLiteral("completed"));
}
