#include <gtest/gtest.h>

#include <memory>

#include "harness/interfaces.h"
#include "harness/tool_registry.h"
#include "core/error_codes.h"
#include "core/types.h"

using namespace act::harness;
using namespace act::core;

namespace
{

// Minimal stub tool for testing
class StubTool : public ITool
{
public:
    explicit StubTool(QString name,
                      PermissionLevel level = PermissionLevel::Read)
        : m_name(std::move(name))
        , m_level(level)
    {
    }

    [[nodiscard]] QString name() const override { return m_name; }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("Stub tool: ") + m_name;
    }
    [[nodiscard]] QJsonObject schema() const override
    {
        return {};
    }

    ToolResult execute(const QJsonObject &params) override
    {
        m_executeCount++;
        return ToolResult::ok(QStringLiteral("executed ") + m_name);
    }

    [[nodiscard]] PermissionLevel permissionLevel() const override
    {
        return m_level;
    }

    [[nodiscard]] bool isThreadSafe() const override { return true; }

    int executeCount() const { return m_executeCount; }

private:
    QString m_name;
    PermissionLevel m_level;
    int m_executeCount = 0;
};

} // anonymous namespace

// --- Registration ---

TEST(ToolRegistryTest, RegisterAndRetrieveTool)
{
    ToolRegistry registry;
    auto tool = std::make_unique<StubTool>(QStringLiteral("test_tool"));
    const StubTool *rawPtr = tool.get();

    registry.registerTool(std::move(tool));

    auto *retrieved = registry.getTool(QStringLiteral("test_tool"));
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name(), QStringLiteral("test_tool"));
    EXPECT_EQ(retrieved, rawPtr);
}

TEST(ToolRegistryTest, RegisterNullToolIsIgnored)
{
    ToolRegistry registry;
    registry.registerTool(nullptr);
    EXPECT_EQ(registry.size(), 0u);
}

TEST(ToolRegistryTest, RegisterMultipleTools)
{
    ToolRegistry registry;
    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("tool_a")));
    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("tool_b")));
    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("tool_c")));

    EXPECT_EQ(registry.size(), 3u);
    EXPECT_NE(registry.getTool(QStringLiteral("tool_a")), nullptr);
    EXPECT_NE(registry.getTool(QStringLiteral("tool_b")), nullptr);
    EXPECT_NE(registry.getTool(QStringLiteral("tool_c")), nullptr);
}

// --- Unregistration ---

TEST(ToolRegistryTest, UnregisterExistingTool)
{
    ToolRegistry registry;
    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("to_remove")));

    registry.unregisterTool(QStringLiteral("to_remove"));
    EXPECT_EQ(registry.size(), 0u);
    EXPECT_EQ(registry.getTool(QStringLiteral("to_remove")), nullptr);
}

TEST(ToolRegistryTest, UnregisterNonExistentToolDoesNotCrash)
{
    ToolRegistry registry;
    registry.unregisterTool(QStringLiteral("does_not_exist"));
    EXPECT_EQ(registry.size(), 0u);
}

// --- Lookup ---

TEST(ToolRegistryTest, GetNonExistentToolReturnsNull)
{
    ToolRegistry registry;
    auto *tool = registry.getTool(QStringLiteral("ghost"));
    EXPECT_EQ(tool, nullptr);
}

TEST(ToolRegistryTest, HasTool)
{
    ToolRegistry registry;
    EXPECT_FALSE(registry.hasTool(QStringLiteral("nope")));

    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("yep")));
    EXPECT_TRUE(registry.hasTool(QStringLiteral("yep")));
}

// --- List ---

TEST(ToolRegistryTest, ListToolsReturnsAllNames)
{
    ToolRegistry registry;
    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("alpha")));
    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("beta")));

    auto names = registry.listTools();
    EXPECT_EQ(names.size(), 2);
    EXPECT_TRUE(names.contains(QStringLiteral("alpha")));
    EXPECT_TRUE(names.contains(QStringLiteral("beta")));
}

TEST(ToolRegistryTest, ListToolsEmptyWhenNoneRegistered)
{
    ToolRegistry registry;
    auto names = registry.listTools();
    EXPECT_TRUE(names.isEmpty());
}

// --- Execute ---

TEST(ToolRegistryTest, ExecuteCallsCorrectTool)
{
    ToolRegistry registry;
    auto tool = std::make_unique<StubTool>(QStringLiteral("my_tool"));
    StubTool *rawPtr = tool.get();
    registry.registerTool(std::move(tool));

    auto result = registry.execute(QStringLiteral("my_tool"), {});
    EXPECT_TRUE(result.success);
    EXPECT_EQ(rawPtr->executeCount(), 1);
}

TEST(ToolRegistryTest, ExecuteNonExistentToolReturnsError)
{
    ToolRegistry registry;
    auto result = registry.execute(QStringLiteral("missing"), {});

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::TOOL_NOT_FOUND);
}

TEST(ToolRegistryTest, ExecutePassesParamsThrough)
{
    ToolRegistry registry;

    // A tool that checks params
    class ParamCheckTool : public ITool
    {
    public:
        [[nodiscard]] QString name() const override { return QStringLiteral("param_check"); }
        [[nodiscard]] QString description() const override { return {}; }
        [[nodiscard]] QJsonObject schema() const override { return {}; }
        ToolResult execute(const QJsonObject &params) override
        {
            capturedParams = params;
            return ToolResult::ok(QStringLiteral("ok"));
        }
        [[nodiscard]] PermissionLevel permissionLevel() const override
        {
            return PermissionLevel::Read;
        }
        QJsonObject capturedParams;
    };

    auto tool = std::make_unique<ParamCheckTool>();
    ParamCheckTool *rawPtr = tool.get();
    registry.registerTool(std::move(tool));

    QJsonObject params;
    params[QStringLiteral("key")] = QStringLiteral("value");
    auto result = registry.execute(QStringLiteral("param_check"), params);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(rawPtr->capturedParams[QStringLiteral("key")].toString(),
              QStringLiteral("value"));
}

TEST(ToolRegistryTest, SizeReflectsRegistrations)
{
    ToolRegistry registry;
    EXPECT_EQ(registry.size(), 0u);

    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("t1")));
    EXPECT_EQ(registry.size(), 1u);

    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("t2")));
    EXPECT_EQ(registry.size(), 2u);

    registry.unregisterTool(QStringLiteral("t1"));
    EXPECT_EQ(registry.size(), 1u);
}

// --- Re-registration overwrites ---

TEST(ToolRegistryTest, ReRegisterOverwritesPrevious)
{
    ToolRegistry registry;
    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("dup")));
    registry.registerTool(std::make_unique<StubTool>(QStringLiteral("dup")));

    EXPECT_EQ(registry.size(), 1u);
    EXPECT_TRUE(registry.hasTool(QStringLiteral("dup")));
}
