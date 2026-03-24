#include <gtest/gtest.h>

#include <QJsonDocument>

#include "services/openai_compat_converter.h"

using namespace act::services;

class OpenAICompatConverterTest : public ::testing::Test
{
protected:
    static QList<act::core::LLMMessage> makeConversation()
    {
        QList<act::core::LLMMessage> msgs;

        act::core::LLMMessage sys;
        sys.role = act::core::MessageRole::System;
        sys.content = QStringLiteral("You are a helpful assistant.");
        msgs.append(sys);

        act::core::LLMMessage user;
        user.role = act::core::MessageRole::User;
        user.content = QStringLiteral("What files are in src/?");
        msgs.append(user);

        return msgs;
    }
};

TEST_F(OpenAICompatConverterTest, ToRequestIncludesSystemAsFirstMessage)
{
    auto msgs = makeConversation();
    auto request = OpenAICompatConverter::toRequest(msgs, "glm-4");

    auto messages = request[QStringLiteral("messages")].toArray();
    ASSERT_EQ(messages.size(), 2);
    EXPECT_EQ(messages[0][QStringLiteral("role")].toString(),
              QStringLiteral("system"));
    EXPECT_EQ(messages[0][QStringLiteral("content")].toString(),
              QStringLiteral("You are a helpful assistant."));
    EXPECT_EQ(messages[1][QStringLiteral("role")].toString(),
              QStringLiteral("user"));
}

TEST_F(OpenAICompatConverterTest, ToRequestIncludesModel)
{
    auto msgs = makeConversation();
    auto request = OpenAICompatConverter::toRequest(msgs, "glm-4-flash");

    EXPECT_EQ(request[QStringLiteral("model")].toString(), QStringLiteral("glm-4-flash"));
}

TEST_F(OpenAICompatConverterTest, ToRequestStreamingEnabled)
{
    auto msgs = makeConversation();
    auto request = OpenAICompatConverter::toRequest(msgs, "model");

    EXPECT_TRUE(request[QStringLiteral("stream")].toBool());
}

TEST_F(OpenAICompatConverterTest, ToolResultMessageConverted)
{
    QList<act::core::LLMMessage> msgs;

    act::core::LLMMessage assistant;
    assistant.role = act::core::MessageRole::Assistant;
    assistant.content = QString();
    assistant.toolCall.id = QStringLiteral("call_123");
    assistant.toolCall.name = QStringLiteral("read_file");
    assistant.toolCall.params = QJsonObject{{QStringLiteral("path"), QStringLiteral("src/main.cpp")}};
    msgs.append(assistant);

    act::core::LLMMessage toolResult;
    toolResult.role = act::core::MessageRole::Tool;
    toolResult.toolCallId = QStringLiteral("call_123");
    toolResult.content = QStringLiteral("// file contents");
    msgs.append(toolResult);

    auto messages = OpenAICompatConverter::buildMessages(msgs);

    ASSERT_EQ(messages.size(), 2);

    // Assistant with tool_calls
    auto assistantMsg = messages[0].toObject();
    EXPECT_EQ(assistantMsg[QStringLiteral("role")].toString(),
              QStringLiteral("assistant"));
    EXPECT_TRUE(assistantMsg.contains(QStringLiteral("tool_calls")));
    auto toolCalls = assistantMsg[QStringLiteral("tool_calls")].toArray();
    ASSERT_EQ(toolCalls.size(), 1);
    EXPECT_EQ(toolCalls[0].toObject()[QStringLiteral("id")].toString(),
              QStringLiteral("call_123"));

    // Tool result
    auto toolMsg = messages[1].toObject();
    EXPECT_EQ(toolMsg[QStringLiteral("role")].toString(),
              QStringLiteral("tool"));
    EXPECT_EQ(toolMsg[QStringLiteral("tool_call_id")].toString(),
              QStringLiteral("call_123"));
}

TEST_F(OpenAICompatConverterTest, ToolToDefinition)
{
    QJsonObject schema;
    QJsonObject props;
    props[QStringLiteral("path")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")}
    };
    schema[QStringLiteral("properties")] = props;

    auto def = OpenAICompatConverter::toolToDefinition(
        QStringLiteral("read_file"),
        QStringLiteral("Read a file"),
        schema);

    EXPECT_EQ(def[QStringLiteral("type")].toString(), QStringLiteral("function"));
    auto fn = def[QStringLiteral("function")].toObject();
    EXPECT_EQ(fn[QStringLiteral("name")].toString(), QStringLiteral("read_file"));
    EXPECT_TRUE(fn.contains(QStringLiteral("parameters")));
}

TEST_F(OpenAICompatConverterTest, AuthHeaders)
{
    auto headers = OpenAICompatConverter::authHeaders("key-123");

    EXPECT_EQ(headers.value(QStringLiteral("Authorization")),
              QStringLiteral("Bearer key-123"));
    EXPECT_EQ(headers.value(QStringLiteral("content-type")),
              QStringLiteral("application/json"));
}

TEST_F(OpenAICompatConverterTest, ParseSseTextDelta)
{
    OpenAICompatConverter::ParsedResponse response;
    QString data = QStringLiteral(
        "{\"id\":\"chatcmpl-123\",\"object\":\"chat.completion.chunk\","
        "\"choices\":[{\"index\":0,\"delta\":{\"content\":\"Hello\"},"
        "\"finish_reason\":null}]}");

    bool ok = OpenAICompatConverter::parseSseEvent(data, response);
    EXPECT_TRUE(ok);
    EXPECT_EQ(response.text, QStringLiteral("Hello"));
}

TEST_F(OpenAICompatConverterTest, ParseSseDoneReturnsFalse)
{
    OpenAICompatConverter::ParsedResponse response;
    bool ok = OpenAICompatConverter::parseSseEvent("[DONE]", response);
    EXPECT_FALSE(ok);
}

TEST_F(OpenAICompatConverterTest, ParseSseToolCallDelta)
{
    OpenAICompatConverter::ParsedResponse response;
    QString data = QStringLiteral(
        "{\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,"
        "\"id\":\"call_abc\",\"type\":\"function\",\"function\":{\"name\":\"read_file\","
        "\"arguments\":\"\"}}]}}]}");

    bool ok = OpenAICompatConverter::parseSseEvent(data, response);
    EXPECT_TRUE(ok);
    ASSERT_EQ(response.toolCalls.size(), 1);
    EXPECT_EQ(response.toolCalls[0].id, QStringLiteral("call_abc"));
    EXPECT_EQ(response.toolCalls[0].name, QStringLiteral("read_file"));
}

TEST_F(OpenAICompatConverterTest, FinalizeAddsStopReason)
{
    OpenAICompatConverter::ParsedResponse accumulated;
    accumulated.text = QStringLiteral("Hello");
    // No finish_reason

    auto finalized = OpenAICompatConverter::finalize(accumulated);
    EXPECT_EQ(finalized.finishReason, QStringLiteral("stop"));
    EXPECT_EQ(finalized.text, QStringLiteral("Hello"));
}
