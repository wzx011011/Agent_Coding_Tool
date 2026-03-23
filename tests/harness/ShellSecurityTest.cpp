#include <gtest/gtest.h>

#include <QTemporaryDir>

#include "core/error_codes.h"
#include "harness/permission_manager.h"
#include "harness/tools/shell_exec_tool.h"
#include "infrastructure/interfaces.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

// Mock IProcess that completes immediately
class MockProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString & /*command*/,
                 const QStringList & /*args*/,
                 std::function<void(int, QString)> callback,
                 int /*timeoutMs*/ = 30000) override
    {
        callback(0, QStringLiteral("mock output"));
    }

    void cancel() override {}
};

// ============================================================
// ShellExecTool Tests
// ============================================================

class ShellExecToolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tmpDir.emplace();
        mockProc = std::make_unique<MockProcess>();
        tool = std::make_unique<ShellExecTool>(*mockProc, tmpDir->path());
    }

    void TearDown() override
    {
        tool.reset();
        mockProc.reset();
        tmpDir.reset();
    }

    std::optional<QTemporaryDir> tmpDir;
    std::unique_ptr<MockProcess> mockProc;
    std::unique_ptr<ShellExecTool> tool;
};

TEST_F(ShellExecToolTest, NameAndMetadata)
{
    EXPECT_EQ(tool->name(), QStringLiteral("shell_exec"));
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Exec);
    EXPECT_FALSE(tool->isThreadSafe());
}

TEST_F(ShellExecToolTest, DefaultDenyListBlocks)
{
    // "rm -rf /" is in the default deny list
    QJsonObject params;
    params[QStringLiteral("command")] = QStringLiteral("rm -rf /");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::COMMAND_BLOCKED);
}

TEST_F(ShellExecToolTest, DefaultDenyListBlocksMkfs)
{
    QJsonObject params;
    params[QStringLiteral("command")] = QStringLiteral("mkfs.ext4 /dev/sda1");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::COMMAND_BLOCKED);
}

TEST_F(ShellExecToolTest, CustomDenyListEntry)
{
    tool->addToDenylist(QStringLiteral("dangerous_cmd"));

    QJsonObject params;
    params[QStringLiteral("command")] = QStringLiteral("dangerous_cmd --force");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::COMMAND_BLOCKED);
}

TEST_F(ShellExecToolTest, MissingCommand)
{
    QJsonObject params;

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

TEST_F(ShellExecToolTest, EmptyCommand)
{
    QJsonObject params;
    params[QStringLiteral("command")] = QStringLiteral("");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

TEST_F(ShellExecToolTest, AllowlistBlocksNonMatching)
{
    tool->addToAllowlist(QStringLiteral("git"));
    tool->addToAllowlist(QStringLiteral("cmake"));

    QJsonObject params;
    params[QStringLiteral("command")] = QStringLiteral("python script.py");

    auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::COMMAND_BLOCKED);
}

// ============================================================
// PermissionManager Integration Tests
// ============================================================

TEST(PermissionIntegration, WriteToolRequiresPermission)
{
    PermissionManager pm;
    EXPECT_FALSE(pm.isAutoApproved(PermissionLevel::Write));
    EXPECT_FALSE(pm.isAutoApproved(PermissionLevel::Exec));
}

TEST(PermissionIntegration, DenyListBlocksTool)
{
    PermissionManager pm;
    pm.addToDenyList(QStringLiteral("shell_exec"));

    auto result = pm.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("shell_exec"),
        QStringLiteral("blocked"));
    EXPECT_EQ(result, PermissionManager::Decision::Denied);
}
