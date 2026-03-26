#include "core/enums.h"
#include "core/error_codes.h"
#include "core/runtime_event.h"
#include "core/types.h"
#include <gtest/gtest.h>

using namespace act::core;

// --- Enums ---

TEST(PermissionLevelTest, HasExpectedValues) {
    EXPECT_EQ(static_cast<int>(PermissionLevel::Read), 0);
    EXPECT_EQ(static_cast<int>(PermissionLevel::Write), 1);
    EXPECT_EQ(static_cast<int>(PermissionLevel::Exec), 2);
    EXPECT_EQ(static_cast<int>(PermissionLevel::Network), 3);
    EXPECT_EQ(static_cast<int>(PermissionLevel::Destructive), 4);
}

TEST(TaskStateTest, HasExpectedValues) {
    EXPECT_EQ(static_cast<int>(TaskState::Idle), 0);
    EXPECT_EQ(static_cast<int>(TaskState::Running), 1);
    EXPECT_EQ(static_cast<int>(TaskState::Completed), 7);
    EXPECT_EQ(static_cast<int>(TaskState::Failed), 6);
}

TEST(MessageRoleTest, HasExpectedValues) {
    EXPECT_EQ(static_cast<int>(MessageRole::System), 0);
    EXPECT_EQ(static_cast<int>(MessageRole::User), 1);
    EXPECT_EQ(static_cast<int>(MessageRole::Assistant), 2);
    EXPECT_EQ(static_cast<int>(MessageRole::Tool), 3);
}

// --- Error Codes ---

TEST(ErrorCodesTest, ProviderErrorsAreDefined) {
    EXPECT_STREQ(errors::PROVIDER_TIMEOUT, "PROVIDER_TIMEOUT");
    EXPECT_STREQ(errors::AUTH_ERROR, "AUTH_ERROR");
    EXPECT_STREQ(errors::RATE_LIMIT, "RATE_LIMIT");
    EXPECT_STREQ(errors::NO_PROVIDER, "NO_PROVIDER");
}

TEST(ErrorCodesTest, FileErrorsAreDefined) {
    EXPECT_STREQ(errors::FILE_NOT_FOUND, "FILE_NOT_FOUND");
    EXPECT_STREQ(errors::OUTSIDE_WORKSPACE, "OUTSIDE_WORKSPACE");
    EXPECT_STREQ(errors::BINARY_FILE, "BINARY_FILE");
    EXPECT_STREQ(errors::PERMISSION_DENIED, "PERMISSION_DENIED");
}

TEST(ErrorCodesTest, ToolErrorsAreDefined) {
    EXPECT_STREQ(errors::TOOL_NOT_FOUND, "TOOL_NOT_FOUND");
    EXPECT_STREQ(errors::INVALID_PARAMS, "INVALID_PARAMS");
    EXPECT_STREQ(errors::INVALID_PATTERN, "INVALID_PATTERN");
    EXPECT_STREQ(errors::STRING_NOT_FOUND, "STRING_NOT_FOUND");
    EXPECT_STREQ(errors::AMBIGUOUS_MATCH, "AMBIGUOUS_MATCH");
}

TEST(ErrorCodesTest, ShellAndGitErrorsAreDefined) {
    EXPECT_STREQ(errors::TIMEOUT, "TIMEOUT");
    EXPECT_STREQ(errors::COMMAND_FAILED, "COMMAND_FAILED");
    EXPECT_STREQ(errors::COMMAND_BLOCKED, "COMMAND_BLOCKED");
    EXPECT_STREQ(errors::NOT_GIT_REPO, "NOT_GIT_REPO");
}

// --- ToolResult ---

TEST(ToolResultTest, OkFactoryCreatesSuccess) {
    auto result = ToolResult::ok(QStringLiteral("file contents"));
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, QStringLiteral("file contents"));
    EXPECT_TRUE(result.error.isEmpty());
    EXPECT_TRUE(result.errorCode.isEmpty());
}

TEST(ToolResultTest, OkFactoryWithMetadata) {
    QJsonObject meta;
    meta[QStringLiteral("lines")] = 42;
    auto result = ToolResult::ok(QStringLiteral("output"), meta);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.metadata[QStringLiteral("lines")].toInt(), 42);
}

TEST(ToolResultTest, ErrFactoryCreatesFailure) {
    auto result = ToolResult::err(errors::FILE_NOT_FOUND, QStringLiteral("file.txt not found"));
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::FILE_NOT_FOUND);
    EXPECT_EQ(result.error, QStringLiteral("file.txt not found"));
    EXPECT_TRUE(result.output.isEmpty());
}

// --- LLMMessage ---

TEST(LLMMessageTest, DefaultIsUserRole) {
    LLMMessage msg;
    EXPECT_EQ(msg.role, MessageRole::User);
    EXPECT_TRUE(msg.content.isEmpty());
}

TEST(LLMMessageTest, CanConstructAssistantMessage) {
    LLMMessage msg;
    msg.role = MessageRole::Assistant;
    msg.content = QStringLiteral("I'll read the file.");
    msg.toolCall.id = QStringLiteral("call_123");
    msg.toolCall.name = QStringLiteral("FileReadTool");
    EXPECT_EQ(msg.role, MessageRole::Assistant);
    EXPECT_EQ(msg.toolCall.name, QStringLiteral("FileReadTool"));
}

TEST(LLMMessageTest, CanConstructToolMessage) {
    LLMMessage msg;
    msg.role = MessageRole::Tool;
    msg.content = QStringLiteral("file contents here");
    msg.toolCallId = QStringLiteral("call_123");
    EXPECT_EQ(msg.role, MessageRole::Tool);
    EXPECT_EQ(msg.toolCallId, QStringLiteral("call_123"));
}

// --- PermissionRequest ---

TEST(PermissionRequestTest, DefaultGeneratesUuid) {
    PermissionRequest req;
    EXPECT_FALSE(req.id.isEmpty());
    EXPECT_EQ(req.level, PermissionLevel::Read);
    // UUID format: 8-4-4-4-12 hex chars (no braces)
    EXPECT_EQ(req.id.length(), 36);
}

TEST(PermissionRequestTest, CanSetFields) {
    PermissionRequest req;
    req.level = PermissionLevel::Write;
    req.toolName = QStringLiteral("FileWriteTool");
    req.description = QStringLiteral("Overwrite main.cpp");
    EXPECT_EQ(req.level, PermissionLevel::Write);
    EXPECT_EQ(req.toolName, QStringLiteral("FileWriteTool"));
    EXPECT_EQ(req.description, QStringLiteral("Overwrite main.cpp"));
}

// --- RuntimeEvent ---

TEST(RuntimeEventTest, StreamTokenEvent) {
    auto ev = RuntimeEvent::streamToken(QStringLiteral("Hello"));
    EXPECT_EQ(ev.type, EventType::StreamToken);
    EXPECT_EQ(ev.data[QStringLiteral("token")].toString(), QStringLiteral("Hello"));
}

TEST(RuntimeEventTest, ToolCallEvent) {
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("main.cpp");
    auto ev = RuntimeEvent::toolCall(QStringLiteral("FileReadTool"), params);
    EXPECT_EQ(ev.type, EventType::ToolCallStarted);
    EXPECT_EQ(ev.data[QStringLiteral("tool")].toString(), QStringLiteral("FileReadTool"));
    auto paramsVal = ev.data[QStringLiteral("params")].toObject();
    EXPECT_EQ(paramsVal[QStringLiteral("path")].toString(), QStringLiteral("main.cpp"));
}

TEST(RuntimeEventTest, PermissionRequestEvent) {
    auto ev = RuntimeEvent::permissionRequest(QStringLiteral("req_001"), QStringLiteral("FileWriteTool"),
                                              PermissionLevel::Write);
    EXPECT_EQ(ev.type, EventType::PermissionRequested);
    EXPECT_EQ(ev.data[QStringLiteral("id")].toString(), QStringLiteral("req_001"));
    EXPECT_EQ(ev.data[QStringLiteral("tool")].toString(), QStringLiteral("FileWriteTool"));
    EXPECT_EQ(ev.data[QStringLiteral("level")].toInt(), static_cast<int>(PermissionLevel::Write));
}

TEST(RuntimeEventTest, PermissionResponseEvent) {
    auto ev = RuntimeEvent::permissionResponse(QStringLiteral("req_001"), true);
    EXPECT_EQ(ev.type, EventType::PermissionResolved);
    EXPECT_EQ(ev.data[QStringLiteral("approved")].toBool(), true);
}

TEST(RuntimeEventTest, TaskStateEvent) {
    auto ev = RuntimeEvent::taskState(TaskState::Completed, QStringLiteral("Done"));
    EXPECT_EQ(ev.type, EventType::TaskStateChanged);
    EXPECT_EQ(ev.data[QStringLiteral("state")].toInt(), static_cast<int>(TaskState::Completed));
    EXPECT_EQ(ev.data[QStringLiteral("summary")].toString(), QStringLiteral("Done"));
}

TEST(RuntimeEventTest, TaskStateEventWithoutSummary) {
    auto ev = RuntimeEvent::taskState(TaskState::Running);
    EXPECT_EQ(ev.type, EventType::TaskStateChanged);
    EXPECT_EQ(ev.data[QStringLiteral("state")].toInt(), static_cast<int>(TaskState::Running));
    EXPECT_FALSE(ev.data.contains(QStringLiteral("summary")));
}

TEST(RuntimeEventTest, ErrorEvent) {
    auto ev = RuntimeEvent::error(errors::PROVIDER_TIMEOUT, QStringLiteral("Connection timed out"));
    EXPECT_EQ(ev.type, EventType::ErrorOccurred);
    EXPECT_EQ(ev.data[QStringLiteral("code")].toString(), errors::PROVIDER_TIMEOUT);
    EXPECT_EQ(ev.data[QStringLiteral("message")].toString(), QStringLiteral("Connection timed out"));
}
