#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>
#include <memory>

#include "harness/interfaces.h"
#include "harness/tool_registry.h"
#include "core/types.h"

using namespace act::core;
using namespace act::harness;

// ============================================================
// Stub tools that represent future tool implementations.
// These validate that ToolRegistry can register tools with
// the expected names, permission levels, and schemas.
// ============================================================

/// Stub for the planned WebFetchTool (network-level permission).
class WebFetchToolStub : public ITool
{
public:
    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("web_fetch");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("Fetch content from a URL");
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        QJsonObject schema;
        schema[QStringLiteral("type")] = QStringLiteral("object");
        QJsonObject props;
        props[QStringLiteral("url")] = QStringLiteral("string");
        schema[QStringLiteral("properties")] = props;
        return schema;
    }
    ToolResult execute(const QJsonObject & /*params*/) override
    {
        return ToolResult::ok(QStringLiteral("stubbed fetch"));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Network;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

/// Stub for the planned SubagentTool (exec-level, spawns sub-agent).
class SubagentToolStub : public ITool
{
public:
    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("subagent");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("Spawn a sub-agent for a sub-task");
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        QJsonObject schema;
        schema[QStringLiteral("type")] = QStringLiteral("object");
        QJsonObject props;
        props[QStringLiteral("task")] = QStringLiteral("string");
        props[QStringLiteral("type")] = QStringLiteral("string");
        schema[QStringLiteral("properties")] = props;
        return schema;
    }
    ToolResult execute(const QJsonObject & /*params*/) override
    {
        return ToolResult::ok(QStringLiteral("stubbed subagent"));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Exec;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

/// Stub for the planned TodoWriteTool (write-level, manages task lists).
class TodoWriteToolStub : public ITool
{
public:
    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("todo_write");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("Write or update a todo list");
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        QJsonObject schema;
        schema[QStringLiteral("type")] = QStringLiteral("object");
        QJsonObject props;
        props[QStringLiteral("todos")] = QStringLiteral("array");
        schema[QStringLiteral("properties")] = props;
        return schema;
    }
    ToolResult execute(const QJsonObject & /*params*/) override
    {
        return ToolResult::ok(QStringLiteral("stubbed todo write"));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Write;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

/// Stub for the planned SkillTool (read-level, loads/invokes skills).
class SkillToolStub : public ITool
{
public:
    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("skill");
    }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("Load or invoke a named skill");
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        QJsonObject schema;
        schema[QStringLiteral("type")] = QStringLiteral("object");
        QJsonObject props;
        props[QStringLiteral("name")] = QStringLiteral("string");
        schema[QStringLiteral("properties")] = props;
        return schema;
    }
    ToolResult execute(const QJsonObject & /*params*/) override
    {
        return ToolResult::ok(QStringLiteral("stubbed skill"));
    }
    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return PermissionLevel::Read;
    }
    [[nodiscard]] bool isThreadSafe() const override { return true; }
};

// ============================================================
// WebFetchTool registration tests
// ============================================================

class WebFetchToolRegistrationTest : public ::testing::Test
{
protected:
    ToolRegistry registry;
};

TEST_F(WebFetchToolRegistrationTest, RegistersSuccessfully)
{
    registry.registerTool(std::make_unique<WebFetchToolStub>());
    EXPECT_TRUE(registry.hasTool(QStringLiteral("web_fetch")));
    EXPECT_EQ(registry.size(), 1u);
}

TEST_F(WebFetchToolRegistrationTest, HasCorrectPermissionLevel)
{
    registry.registerTool(std::make_unique<WebFetchToolStub>());
    auto *tool = registry.getTool(QStringLiteral("web_fetch"));
    ASSERT_NE(tool, nullptr);
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Network);
}

TEST_F(WebFetchToolRegistrationTest, HasNonEmptySchema)
{
    registry.registerTool(std::make_unique<WebFetchToolStub>());
    auto *tool = registry.getTool(QStringLiteral("web_fetch"));
    ASSERT_NE(tool, nullptr);
    auto schema = tool->schema();
    EXPECT_FALSE(schema.isEmpty());
    EXPECT_TRUE(schema.contains(QStringLiteral("type")));
    EXPECT_TRUE(schema.contains(QStringLiteral("properties")));
}

TEST_F(WebFetchToolRegistrationTest, ExecutesAndReturnsOk)
{
    registry.registerTool(std::make_unique<WebFetchToolStub>());
    auto result = registry.execute(QStringLiteral("web_fetch"), {});
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.output.isEmpty());
}

// ============================================================
// SubagentTool registration tests
// ============================================================

class SubagentToolRegistrationTest : public ::testing::Test
{
protected:
    ToolRegistry registry;
};

TEST_F(SubagentToolRegistrationTest, RegistersSuccessfully)
{
    registry.registerTool(std::make_unique<SubagentToolStub>());
    EXPECT_TRUE(registry.hasTool(QStringLiteral("subagent")));
    EXPECT_EQ(registry.size(), 1u);
}

TEST_F(SubagentToolRegistrationTest, HasCorrectPermissionLevel)
{
    registry.registerTool(std::make_unique<SubagentToolStub>());
    auto *tool = registry.getTool(QStringLiteral("subagent"));
    ASSERT_NE(tool, nullptr);
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Exec);
}

TEST_F(SubagentToolRegistrationTest, HasSchemaWithTaskAndType)
{
    registry.registerTool(std::make_unique<SubagentToolStub>());
    auto *tool = registry.getTool(QStringLiteral("subagent"));
    ASSERT_NE(tool, nullptr);
    auto props = tool->schema()[QStringLiteral("properties")].toObject();
    EXPECT_TRUE(props.contains(QStringLiteral("task")));
    EXPECT_TRUE(props.contains(QStringLiteral("type")));
}

// ============================================================
// TodoWriteTool registration tests
// ============================================================

class TodoWriteToolRegistrationTest : public ::testing::Test
{
protected:
    ToolRegistry registry;
};

TEST_F(TodoWriteToolRegistrationTest, RegistersSuccessfully)
{
    registry.registerTool(std::make_unique<TodoWriteToolStub>());
    EXPECT_TRUE(registry.hasTool(QStringLiteral("todo_write")));
    EXPECT_EQ(registry.size(), 1u);
}

TEST_F(TodoWriteToolRegistrationTest, HasWritePermissionLevel)
{
    registry.registerTool(std::make_unique<TodoWriteToolStub>());
    auto *tool = registry.getTool(QStringLiteral("todo_write"));
    ASSERT_NE(tool, nullptr);
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Write);
}

// ============================================================
// SkillTool registration tests
// ============================================================

class SkillToolRegistrationTest : public ::testing::Test
{
protected:
    ToolRegistry registry;
};

TEST_F(SkillToolRegistrationTest, RegistersSuccessfully)
{
    registry.registerTool(std::make_unique<SkillToolStub>());
    EXPECT_TRUE(registry.hasTool(QStringLiteral("skill")));
    EXPECT_EQ(registry.size(), 1u);
}

TEST_F(SkillToolRegistrationTest, HasReadPermissionLevel)
{
    registry.registerTool(std::make_unique<SkillToolStub>());
    auto *tool = registry.getTool(QStringLiteral("skill"));
    ASSERT_NE(tool, nullptr);
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Read);
}

// ============================================================
// Multi-tool registration: all stubs coexist
// ============================================================

class MultiToolRegistrationTest : public ::testing::Test
{
protected:
    ToolRegistry registry;

    void SetUp() override
    {
        registry.registerTool(std::make_unique<WebFetchToolStub>());
        registry.registerTool(std::make_unique<SubagentToolStub>());
        registry.registerTool(std::make_unique<TodoWriteToolStub>());
        registry.registerTool(std::make_unique<SkillToolStub>());
    }
};

TEST_F(MultiToolRegistrationTest, AllToolsRegistered)
{
    EXPECT_EQ(registry.size(), 4u);
    EXPECT_TRUE(registry.hasTool(QStringLiteral("web_fetch")));
    EXPECT_TRUE(registry.hasTool(QStringLiteral("subagent")));
    EXPECT_TRUE(registry.hasTool(QStringLiteral("todo_write")));
    EXPECT_TRUE(registry.hasTool(QStringLiteral("skill")));
}

TEST_F(MultiToolRegistrationTest, ListToolsReturnsAllNames)
{
    auto names = registry.listTools();
    EXPECT_EQ(names.size(), 4);
    EXPECT_TRUE(names.contains(QStringLiteral("web_fetch")));
    EXPECT_TRUE(names.contains(QStringLiteral("subagent")));
    EXPECT_TRUE(names.contains(QStringLiteral("todo_write")));
    EXPECT_TRUE(names.contains(QStringLiteral("skill")));
}

TEST_F(MultiToolRegistrationTest, EachToolHasUniquePermissionLevel)
{
    // Verify that the four tools have distinct permission levels
    auto levels = QList<PermissionLevel>{
        registry.getTool(QStringLiteral("web_fetch"))->permissionLevel(),
        registry.getTool(QStringLiteral("subagent"))->permissionLevel(),
        registry.getTool(QStringLiteral("todo_write"))->permissionLevel(),
        registry.getTool(QStringLiteral("skill"))->permissionLevel(),
    };

    // Network, Exec, Write, Read -- all distinct
    for (int i = 0; i < levels.size(); ++i)
    {
        for (int j = i + 1; j < levels.size(); ++j)
        {
            EXPECT_NE(levels[i], levels[j])
                << "Tools at positions " << i << " and " << j
                << " share the same permission level";
        }
    }
}

TEST_F(MultiToolRegistrationTest, ExecuteEachToolSucceeds)
{
    for (const auto &name : registry.listTools())
    {
        auto result = registry.execute(name, {});
        EXPECT_TRUE(result.success) << "Tool " << name.toStdString() << " failed";
    }
}

TEST_F(MultiToolRegistrationTest, UnregisterOneToolLeavesOthersIntact)
{
    registry.unregisterTool(QStringLiteral("subagent"));
    EXPECT_FALSE(registry.hasTool(QStringLiteral("subagent")));
    EXPECT_EQ(registry.size(), 3u);
    EXPECT_TRUE(registry.hasTool(QStringLiteral("web_fetch")));
    EXPECT_TRUE(registry.hasTool(QStringLiteral("todo_write")));
    EXPECT_TRUE(registry.hasTool(QStringLiteral("skill")));
}

TEST_F(MultiToolRegistrationTest, GetNonExistentToolReturnsNull)
{
    auto *tool = registry.getTool(QStringLiteral("nonexistent"));
    EXPECT_EQ(tool, nullptr);
}

TEST_F(MultiToolRegistrationTest, ExecuteNonExistentToolReturnsError)
{
    auto result = registry.execute(QStringLiteral("nonexistent"), {});
    EXPECT_FALSE(result.success);
}
