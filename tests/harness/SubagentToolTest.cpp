#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include "core/error_codes.h"
#include "framework/subagent_manager.h"
#include "harness/tools/subagent_tool.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

class SubagentToolTest : public ::testing::Test
{
protected:
    act::framework::SubagentManager manager;
    SubagentTool tool{manager};
};

// --- Name, description, permissions ---

TEST_F(SubagentToolTest, NameAndDescription)
{
    EXPECT_EQ(tool.name(), QStringLiteral("run_subagent"));
    EXPECT_FALSE(tool.description().isEmpty());
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Exec);
    EXPECT_FALSE(tool.isThreadSafe());
}

// --- Schema ---

TEST_F(SubagentToolTest, SchemaRequiresTypeAndTask)
{
    auto schema = tool.schema();
    auto required = schema.value(QStringLiteral("required")).toArray();
    ASSERT_EQ(required.size(), 2);

    QStringList requiredFields;
    for (const auto &v : required)
        requiredFields.append(v.toString());

    EXPECT_TRUE(requiredFields.contains(QStringLiteral("type")));
    EXPECT_TRUE(requiredFields.contains(QStringLiteral("task")));

    auto props = schema.value(QStringLiteral("properties")).toObject();
    EXPECT_TRUE(props.contains(QStringLiteral("type")));
    EXPECT_TRUE(props.contains(QStringLiteral("task")));
}

// --- Execute ---

TEST_F(SubagentToolTest, SpawnExploreSubagent)
{
    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("explore");
    params[QStringLiteral("task")] = QStringLiteral("Find all test files");

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.output.indexOf(QStringLiteral("explore")), -1);
    EXPECT_FALSE(
        result.metadata.value(QStringLiteral("subagent_id")).toString().isEmpty());
    EXPECT_EQ(result.metadata.value(QStringLiteral("type")).toString(),
              QStringLiteral("explore"));
    EXPECT_EQ(manager.count(), 1);
}

TEST_F(SubagentToolTest, SpawnCodeSubagent)
{
    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("code");
    params[QStringLiteral("task")] = QStringLiteral("Implement feature X");

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.output.indexOf(QStringLiteral("code")), -1);
    EXPECT_EQ(manager.count(), 1);
}

TEST_F(SubagentToolTest, SpawnMultipleSubagents)
{
    QJsonObject params1;
    params1[QStringLiteral("type")] = QStringLiteral("explore");
    params1[QStringLiteral("task")] = QStringLiteral("Task 1");
    tool.execute(params1);

    QJsonObject params2;
    params2[QStringLiteral("type")] = QStringLiteral("code");
    params2[QStringLiteral("task")] = QStringLiteral("Task 2");
    tool.execute(params2);

    EXPECT_EQ(manager.count(), 2);
}

TEST_F(SubagentToolTest, MissingType)
{
    QJsonObject params;
    params[QStringLiteral("task")] = QStringLiteral("Some task");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
    EXPECT_NE(result.error.indexOf(QStringLiteral("type")), -1);
}

TEST_F(SubagentToolTest, MissingTask)
{
    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("explore");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
    EXPECT_NE(result.error.indexOf(QStringLiteral("task")), -1);
}

TEST_F(SubagentToolTest, InvalidType)
{
    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("invalid_type");
    params[QStringLiteral("task")] = QStringLiteral("Some task");

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
    EXPECT_NE(result.error.indexOf(QStringLiteral("Invalid sub-agent type")), -1);
}

TEST_F(SubagentToolTest, EmptyParams)
{
    QJsonObject params;

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}
