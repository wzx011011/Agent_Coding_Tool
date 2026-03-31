#include <gtest/gtest.h>

#include <QString>
#include <memory>

#include "harness/interfaces.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"
#include "core/types.h"

using namespace act::core;
using namespace act::harness;

// ============================================================
// Stub tool with configurable permission level
// ============================================================

class StubTool : public ITool
{
public:
    explicit StubTool(QString toolName, PermissionLevel level)
        : m_name(std::move(toolName)), m_level(level) {}

    [[nodiscard]] QString name() const override { return m_name; }
    [[nodiscard]] QString description() const override
    {
        return QStringLiteral("Stub tool: %1").arg(m_name);
    }
    [[nodiscard]] QJsonObject schema() const override { return QJsonObject{}; }

    ToolResult execute(const QJsonObject & /*params*/) override
    {
        return ToolResult::ok(QStringLiteral("executed"));
    }

    [[nodiscard]] PermissionLevel permissionLevel() const override { return m_level; }
    [[nodiscard]] bool isThreadSafe() const override { return true; }

private:
    QString m_name;
    PermissionLevel m_level;
};

// ============================================================
// PermissionFlow tests
// ============================================================

class PermissionFlowTest : public ::testing::Test
{
protected:
    PermissionManager permissions;
};

// --- Auto-approve defaults ---

TEST_F(PermissionFlowTest, ReadIsAutoApprovedByDefault)
{
    EXPECT_TRUE(permissions.isAutoApproved(PermissionLevel::Read));
}

TEST_F(PermissionFlowTest, WriteIsNotAutoApprovedByDefault)
{
    EXPECT_FALSE(permissions.isAutoApproved(PermissionLevel::Write));
}

TEST_F(PermissionFlowTest, ExecIsNotAutoApprovedByDefault)
{
    EXPECT_FALSE(permissions.isAutoApproved(PermissionLevel::Exec));
}

TEST_F(PermissionFlowTest, NetworkIsNotAutoApprovedByDefault)
{
    EXPECT_FALSE(permissions.isAutoApproved(PermissionLevel::Network));
}

TEST_F(PermissionFlowTest, DestructiveIsNotAutoApprovedByDefault)
{
    EXPECT_FALSE(permissions.isAutoApproved(PermissionLevel::Destructive));
}

// --- Auto-approve toggle ---

TEST_F(PermissionFlowTest, ToggleAutoApproveWrite)
{
    permissions.setAutoApproved(PermissionLevel::Write, true);
    EXPECT_TRUE(permissions.isAutoApproved(PermissionLevel::Write));
    permissions.setAutoApproved(PermissionLevel::Write, false);
    EXPECT_FALSE(permissions.isAutoApproved(PermissionLevel::Write));
}

TEST_F(PermissionFlowTest, ToggleAutoApproveExec)
{
    permissions.setAutoApproved(PermissionLevel::Exec, true);
    EXPECT_TRUE(permissions.isAutoApproved(PermissionLevel::Exec));
}

TEST_F(PermissionFlowTest, ToggleAutoApproveNetwork)
{
    permissions.setAutoApproved(PermissionLevel::Network, true);
    EXPECT_TRUE(permissions.isAutoApproved(PermissionLevel::Network));
}

// --- checkPermission with auto-approve ---

TEST_F(PermissionFlowTest, AutoApprovedReadReturnsApproved)
{
    auto decision = permissions.checkPermission(
        PermissionLevel::Read,
        QStringLiteral("file_read"),
        QStringLiteral("Read a file"));
    EXPECT_EQ(decision, PermissionManager::Decision::Approved);
}

TEST_F(PermissionFlowTest, NotAutoApprovedWriteTriggersCallback)
{
    bool callbackCalled = false;
    permissions.setPermissionCallback([&callbackCalled](const PermissionRequest &) {
        callbackCalled = true;
        return true;
    });

    auto decision = permissions.checkPermission(
        PermissionLevel::Write,
        QStringLiteral("file_write"),
        QStringLiteral("Write to a file"));
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(decision, PermissionManager::Decision::Approved);
}

TEST_F(PermissionFlowTest, NotAutoApprovedWithoutCallbackReturnsDenied)
{
    // No callback set, Write not auto-approved -> should deny
    auto decision = permissions.checkPermission(
        PermissionLevel::Write,
        QStringLiteral("file_write"),
        QStringLiteral("Write to a file"));
    EXPECT_EQ(decision, PermissionManager::Decision::Denied);
}

// --- Permission callback approval/denial ---

TEST_F(PermissionFlowTest, CallbackReturnsApprovedWhenTrue)
{
    permissions.setPermissionCallback([](const PermissionRequest &) {
        return true;
    });

    auto decision = permissions.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("shell_exec"),
        QStringLiteral("Execute a command"));
    EXPECT_EQ(decision, PermissionManager::Decision::Approved);
}

TEST_F(PermissionFlowTest, CallbackReturnsDeniedWhenFalse)
{
    permissions.setPermissionCallback([](const PermissionRequest &) {
        return false;
    });

    auto decision = permissions.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("shell_exec"),
        QStringLiteral("Execute a command"));
    EXPECT_EQ(decision, PermissionManager::Decision::Denied);
}

// --- Deny list ---

TEST_F(PermissionFlowTest, DenyListBlocksEvenWithCallback)
{
    permissions.setPermissionCallback([](const PermissionRequest &) {
        return true;
    });
    permissions.addToDenyList(QStringLiteral("dangerous_tool"));

    auto decision = permissions.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("dangerous_tool"),
        QStringLiteral("Do something dangerous"));
    EXPECT_EQ(decision, PermissionManager::Decision::Denied);
}

TEST_F(PermissionFlowTest, DenyListBlocksEvenAutoApproved)
{
    permissions.setAutoApproved(PermissionLevel::Read, true);
    permissions.addToDenyList(QStringLiteral("blocked_read_tool"));

    auto decision = permissions.checkPermission(
        PermissionLevel::Read,
        QStringLiteral("blocked_read_tool"),
        QStringLiteral("Read something"));
    EXPECT_EQ(decision, PermissionManager::Decision::Denied);
}

TEST_F(PermissionFlowTest, DenyListDoesNotBlockOtherTools)
{
    permissions.setPermissionCallback([](const PermissionRequest &) {
        return true;
    });
    permissions.addToDenyList(QStringLiteral("bad_tool"));

    auto decision = permissions.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("good_tool"),
        QStringLiteral("Do something good"));
    EXPECT_EQ(decision, PermissionManager::Decision::Approved);
}

TEST_F(PermissionFlowTest, IsDeniedReportsCorrectly)
{
    permissions.addToDenyList(QStringLiteral("tool_a"));
    EXPECT_TRUE(permissions.isDenied(QStringLiteral("tool_a")));
    EXPECT_FALSE(permissions.isDenied(QStringLiteral("tool_b")));
}

// --- Full permission flow with ToolRegistry ---

class PermissionFlowWithRegistryTest : public ::testing::Test
{
protected:
    ToolRegistry registry;
    PermissionManager permissions;

    void SetUp() override
    {
        registry.registerTool(std::make_unique<StubTool>(
            QStringLiteral("read_tool"), PermissionLevel::Read));
        registry.registerTool(std::make_unique<StubTool>(
            QStringLiteral("write_tool"), PermissionLevel::Write));
        registry.registerTool(std::make_unique<StubTool>(
            QStringLiteral("exec_tool"), PermissionLevel::Exec));
        registry.registerTool(std::make_unique<StubTool>(
            QStringLiteral("network_tool"), PermissionLevel::Network));

        permissions.setAutoApproved(PermissionLevel::Read, true);
    }
};

TEST_F(PermissionFlowWithRegistryTest, ReadToolAutoApproved)
{
    auto decision = permissions.checkPermission(
        PermissionLevel::Read,
        QStringLiteral("read_tool"),
        QStringLiteral("Read data"));
    EXPECT_EQ(decision, PermissionManager::Decision::Approved);
}

TEST_F(PermissionFlowWithRegistryTest, WriteToolRequiresExplicitApproval)
{
    bool callbackCalled = false;
    permissions.setPermissionCallback([&callbackCalled](const PermissionRequest &) {
        callbackCalled = true;
        return true;
    });

    auto decision = permissions.checkPermission(
        PermissionLevel::Write,
        QStringLiteral("write_tool"),
        QStringLiteral("Write data"));
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(decision, PermissionManager::Decision::Approved);
}

TEST_F(PermissionFlowWithRegistryTest, ExecToolDeniedByDefault)
{
    auto decision = permissions.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("exec_tool"),
        QStringLiteral("Execute command"));
    EXPECT_EQ(decision, PermissionManager::Decision::Denied);
}

TEST_F(PermissionFlowWithRegistryTest, NetworkToolDeniedByDefault)
{
    auto decision = permissions.checkPermission(
        PermissionLevel::Network,
        QStringLiteral("network_tool"),
        QStringLiteral("Fetch URL"));
    EXPECT_EQ(decision, PermissionManager::Decision::Denied);
}

TEST_F(PermissionFlowWithRegistryTest, PermissionRequestContainsCorrectFields)
{
    PermissionRequest captured;
    permissions.setPermissionCallback([&captured](const PermissionRequest &req) {
        captured = req;
        return true;
    });

    permissions.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("exec_tool"),
        QStringLiteral("Run tests"),
        {{QStringLiteral("command"), QStringLiteral("ctest")}});

    EXPECT_EQ(captured.level, PermissionLevel::Exec);
    EXPECT_EQ(captured.toolName, QStringLiteral("exec_tool"));
    EXPECT_EQ(captured.description, QStringLiteral("Run tests"));
    EXPECT_TRUE(captured.params.contains(QStringLiteral("command")));
    EXPECT_FALSE(captured.id.isEmpty());
}

// --- All permission levels in one test ---

TEST_F(PermissionFlowTest, AllPermissionLevelsCanBeChecked)
{
    permissions.setPermissionCallback([](const PermissionRequest &) {
        return true;
    });

    // Read: auto-approved
    EXPECT_EQ(
        permissions.checkPermission(PermissionLevel::Read,
                                    QStringLiteral("t"), QStringLiteral("d")),
        PermissionManager::Decision::Approved);

    // Write, Exec, Network, Destructive: approved via callback
    for (auto level : {PermissionLevel::Write,
                       PermissionLevel::Exec,
                       PermissionLevel::Network,
                       PermissionLevel::Destructive})
    {
        EXPECT_EQ(
            permissions.checkPermission(level, QStringLiteral("t"), QStringLiteral("d")),
            PermissionManager::Decision::Approved);
    }
}
