#include "framework/interactive_session_state.h"
#include <gtest/gtest.h>

using namespace act::framework;

TEST(InteractiveSessionStateTest, AssistantTokensAccumulateIntoSingleMessage) {
    InteractiveSessionState state;

    state.appendAssistantToken(QStringLiteral("Hello"));
    state.appendAssistantToken(QStringLiteral(", world"));
    state.finalizeAssistantMessage();

    ASSERT_EQ(state.messages().size(), 1);
    EXPECT_EQ(state.messages().front().kind, SessionMessageKind::Assistant);
    EXPECT_EQ(state.messages().front().content, QStringLiteral("Hello, world"));
}

TEST(InteractiveSessionStateTest, FinalizeAssistantUsesFallbackWhenNoTokensStreamed) {
    InteractiveSessionState state;

    state.finalizeAssistantMessage(QStringLiteral("Fallback response"));

    ASSERT_EQ(state.messages().size(), 1);
    EXPECT_EQ(state.messages().front().content, QStringLiteral("Fallback response"));
}

TEST(InteractiveSessionStateTest, PermissionPromptCanBeSetAndCleared) {
    InteractiveSessionState state;

    state.setPermissionPrompt(QStringLiteral("shell_exec"), QStringLiteral("Exec"), QStringLiteral("Run tool"));
    EXPECT_TRUE(state.permissionPrompt().active);
    EXPECT_EQ(state.permissionPrompt().toolName, QStringLiteral("shell_exec"));

    state.clearPermissionPrompt();
    EXPECT_FALSE(state.permissionPrompt().active);
}

TEST(InteractiveSessionStateTest, ClearConversationResetsTransientState) {
    InteractiveSessionState state;
    state.appendUserMessage(QStringLiteral("hello"));
    state.logActivity(QStringLiteral("Submitted"));
    state.setBusy(true);
    state.setStatus(QStringLiteral("Running"));

    state.clearConversation();

    EXPECT_TRUE(state.messages().isEmpty());
    EXPECT_TRUE(state.activityLog().isEmpty());
    EXPECT_EQ(state.status(), QStringLiteral("Idle"));
    EXPECT_FALSE(state.isBusy());
}