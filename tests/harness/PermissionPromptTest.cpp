#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "harness/permission_manager.h"

using namespace act::harness;
using namespace act::core;

class PermissionPromptTest : public ::testing::Test
{
protected:
    PermissionManager manager;
};

TEST_F(PermissionPromptTest, AutoApproveReadReturnsApproved)
{
    manager.setAutoApproved(PermissionLevel::Read, true);
    auto decision = manager.checkPermission(
        PermissionLevel::Read,
        QStringLiteral("test_tool"),
        QStringLiteral("Read test"));
    EXPECT_EQ(decision, PermissionManager::Decision::Approved);
}

TEST_F(PermissionPromptTest, NoCallbackDeniesByDefault)
{
    manager.setAutoApproved(PermissionLevel::Exec, false);
    auto decision = manager.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("shell_exec"),
        QStringLiteral("Execute command"));
    EXPECT_EQ(decision, PermissionManager::Decision::Denied);
}

TEST_F(PermissionPromptTest, CallbackApprovesWhenReturnsTrue)
{
    manager.setAutoApproved(PermissionLevel::Exec, false);
    manager.setPermissionCallback([](const PermissionRequest &request) {
        return true; // Approve all
    });

    auto decision = manager.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("shell_exec"),
        QStringLiteral("Execute command"));
    EXPECT_EQ(decision, PermissionManager::Decision::Approved);
}

TEST_F(PermissionPromptTest, CallbackDeniesWhenReturnsFalse)
{
    manager.setAutoApproved(PermissionLevel::Exec, false);
    manager.setPermissionCallback([](const PermissionRequest &request) {
        return false; // Deny all
    });

    auto decision = manager.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("shell_exec"),
        QStringLiteral("Execute command"));
    EXPECT_EQ(decision, PermissionManager::Decision::Denied);
}

TEST_F(PermissionPromptTest, CallbackReceivesCorrectRequest)
{
    manager.setAutoApproved(PermissionLevel::Write, false);

    PermissionRequest receivedRequest;
    manager.setPermissionCallback([&receivedRequest](const PermissionRequest &request) {
        receivedRequest = request;
        return true;
    });

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("/test/path");

    (void)manager.checkPermission(
        PermissionLevel::Write,
        QStringLiteral("file_write"),
        QStringLiteral("Write file"),
        params);

    EXPECT_EQ(receivedRequest.level, PermissionLevel::Write);
    EXPECT_EQ(receivedRequest.toolName, QStringLiteral("file_write"));
    EXPECT_EQ(receivedRequest.description, QStringLiteral("Write file"));
    EXPECT_EQ(receivedRequest.params[QStringLiteral("path")].toString(),
              QStringLiteral("/test/path"));
}

TEST_F(PermissionPromptTest, DenyListOverridesCallback)
{
    manager.setAutoApproved(PermissionLevel::Exec, false);
    manager.addToDenyList(QStringLiteral("dangerous_tool"));

    manager.setPermissionCallback([](const PermissionRequest &) {
        return true; // Would approve, but deny list takes precedence
    });

    auto decision = manager.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("dangerous_tool"),
        QStringLiteral("Dangerous operation"));
    EXPECT_EQ(decision, PermissionManager::Decision::Denied);
}

TEST_F(PermissionPromptTest, DenyListOverridesAutoApprove)
{
    manager.setAutoApproved(PermissionLevel::Read, true);
    manager.addToDenyList(QStringLiteral("blocked_read"));

    auto decision = manager.checkPermission(
        PermissionLevel::Read,
        QStringLiteral("blocked_read"),
        QStringLiteral("Blocked read"));
    EXPECT_EQ(decision, PermissionManager::Decision::Denied);
}

TEST_F(PermissionPromptTest, CallbackNotCalledForAutoApproved)
{
    manager.setAutoApproved(PermissionLevel::Read, true);

    bool callbackCalled = false;
    manager.setPermissionCallback([&callbackCalled](const PermissionRequest &) {
        callbackCalled = true;
        return true;
    });

    (void)manager.checkPermission(
        PermissionLevel::Read,
        QStringLiteral("auto_tool"),
        QStringLiteral("Auto-approved"));

    EXPECT_FALSE(callbackCalled);
}

TEST_F(PermissionPromptTest, CallbackNotCalledForDeniedTool)
{
    manager.setAutoApproved(PermissionLevel::Exec, false);
    manager.addToDenyList(QStringLiteral("denied_tool"));

    bool callbackCalled = false;
    manager.setPermissionCallback([&callbackCalled](const PermissionRequest &) {
        callbackCalled = true;
        return true;
    });

    (void)manager.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("denied_tool"),
        QStringLiteral("Denied tool"));

    EXPECT_FALSE(callbackCalled);
}

TEST_F(PermissionPromptTest, SelectiveApprovalBasedOnToolName)
{
    manager.setAutoApproved(PermissionLevel::Exec, false);

    manager.setPermissionCallback([](const PermissionRequest &request) {
        // Only approve specific tools
        return request.toolName == QStringLiteral("safe_tool");
    });

    auto safeDecision = manager.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("safe_tool"),
        QStringLiteral("Safe operation"));
    EXPECT_EQ(safeDecision, PermissionManager::Decision::Approved);

    auto unsafeDecision = manager.checkPermission(
        PermissionLevel::Exec,
        QStringLiteral("unsafe_tool"),
        QStringLiteral("Unsafe operation"));
    EXPECT_EQ(unsafeDecision, PermissionManager::Decision::Denied);
}

TEST_F(PermissionPromptTest, SelectiveApprovalBasedOnPermissionLevel)
{
    // Read auto-approved, Write requires confirmation
    manager.setAutoApproved(PermissionLevel::Read, true);
    manager.setAutoApproved(PermissionLevel::Write, false);

    int callbackCount = 0;
    manager.setPermissionCallback([&callbackCount](const PermissionRequest &) {
        callbackCount++;
        return true;
    });

    (void)manager.checkPermission(
        PermissionLevel::Read,
        QStringLiteral("read_tool"),
        QStringLiteral("Read"));
    EXPECT_EQ(callbackCount, 0); // No callback for auto-approved

    (void)manager.checkPermission(
        PermissionLevel::Write,
        QStringLiteral("write_tool"),
        QStringLiteral("Write"));
    EXPECT_EQ(callbackCount, 1); // Callback called for non-auto-approved
}
