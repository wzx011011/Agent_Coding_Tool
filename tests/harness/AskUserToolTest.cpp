#include <gtest/gtest.h>

#include <QJsonArray>

#include "core/error_codes.h"
#include "harness/tools/ask_user_tool.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

TEST(AskUserToolTest, NameAndDescription)
{
    AskUserTool tool;
    EXPECT_EQ(tool.name(), QStringLiteral("ask_user"));
    EXPECT_FALSE(tool.description().isEmpty());
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);
    EXPECT_FALSE(tool.isThreadSafe());
}

TEST(AskUserToolTest, ExecuteWithQuestionReturnsWaitingMarker)
{
    AskUserTool tool;
    QJsonObject params;
    params[QStringLiteral("question")] = QStringLiteral("What is your name?");

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, QStringLiteral("__WAITING_USER_INPUT__"));
    EXPECT_TRUE(result.metadata.value(QStringLiteral("pause_agent")).toBool());
    EXPECT_EQ(result.metadata.value(QStringLiteral("prompt")).toString(),
              QStringLiteral("What is your name?"));
    EXPECT_TRUE(tool.isWaiting());
    EXPECT_EQ(tool.pendingPrompt(), QStringLiteral("What is your name?"));
}

TEST(AskUserToolTest, ExecuteWithQuestionAndOptions)
{
    AskUserTool tool;
    QJsonObject params;
    params[QStringLiteral("question")] = QStringLiteral("Choose a color");
    params[QStringLiteral("options")] =
        QJsonArray{QStringLiteral("red"), QStringLiteral("blue")};

    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.metadata.value(QStringLiteral("pause_agent")).toBool());
}

TEST(AskUserToolTest, MissingQuestionParameter)
{
    AskUserTool tool;
    QJsonObject params;

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
    EXPECT_FALSE(tool.isWaiting());
}

TEST(AskUserToolTest, EmptyQuestionParameter)
{
    AskUserTool tool;
    QJsonObject params;
    params[QStringLiteral("question")] = QString();

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(AskUserToolTest, OnUserInputReturnsTrueWhenWaiting)
{
    AskUserTool tool;
    QJsonObject params;
    params[QStringLiteral("question")] = QStringLiteral("Yes or no?");

    tool.execute(params);
    ASSERT_TRUE(tool.isWaiting());

    bool callbackCalled = false;
    QString callbackResponse;
    tool.setResponseCallback([&](const QString &response) {
        callbackCalled = true;
        callbackResponse = response;
    });

    bool handled = tool.onUserInput(QStringLiteral("yes"));
    EXPECT_TRUE(handled);
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(callbackResponse, QStringLiteral("yes"));
    EXPECT_FALSE(tool.isWaiting());
}

TEST(AskUserToolTest, OnUserInputReturnsFalseWhenNotWaiting)
{
    AskUserTool tool;

    bool handled = tool.onUserInput(QStringLiteral("no"));
    EXPECT_FALSE(handled);
}

TEST(AskUserToolTest, OnUserInputReturnsFalseAfterAlreadyHandled)
{
    AskUserTool tool;
    QJsonObject params;
    params[QStringLiteral("question")] = QStringLiteral("Pick one");

    tool.execute(params);
    tool.onUserInput(QStringLiteral("a"));

    // Second call should return false
    bool handled = tool.onUserInput(QStringLiteral("b"));
    EXPECT_FALSE(handled);
}

TEST(AskUserToolTest, SchemaRequiresQuestion)
{
    AskUserTool tool;
    auto schema = tool.schema();

    auto required = schema.value(QStringLiteral("required")).toArray();
    ASSERT_EQ(required.size(), 1);
    EXPECT_EQ(required.at(0).toString(), QStringLiteral("question"));
}

TEST(AskUserToolTest, SchemaHasOptionsProperty)
{
    AskUserTool tool;
    auto schema = tool.schema();

    auto props = schema.value(QStringLiteral("properties")).toObject();
    EXPECT_TRUE(props.contains(QStringLiteral("question")));
    EXPECT_TRUE(props.contains(QStringLiteral("options")));
}
