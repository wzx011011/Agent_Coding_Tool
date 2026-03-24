#include <gtest/gtest.h>

#include <QJsonDocument>

#include "services/anthropic_converter.h"

using namespace act::services;

class AnthropicConverterTest : public ::testing::Test
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

TEST_F(AnthropicConverterTest, ToRequestExtractsSystem)
{
    auto msgs = makeConversation();
    auto request = AnthropicConverter::toRequest(msgs, "claude-sonnet-4-20250514", 4096);

    EXPECT_TRUE(request.contains(QStringLiteral("system")));
    EXPECT_EQ(request[QStringLiteral("system")].toString(),
              QStringLiteral("You are a helpful assistant."));

    auto messages = request[QStringLiteral("messages")].toArray();
    ASSERT_EQ(messages.size(), 1); // Only user message, system extracted
    EXPECT_EQ(messages[0][QStringLiteral("role")].toString(),
              QStringLiteral("user"));
}

TEST_F(AnthropicConverterTest, ToRequestIncludesModelAndMaxTokens)
{
    auto msgs = makeConversation();
    auto request = AnthropicConverter::toRequest(msgs, "test-model", 8192);

    EXPECT_EQ(request[QStringLiteral("model")].toString(), QStringLiteral("test-model"));
    EXPECT_EQ(request[QStringLiteral("max_tokens")].toInt(), 8192);
}

TEST_F(AnthropicConverterTest, ToRequestStreamingEnabled)
{
    auto msgs = makeConversation();
    auto request = AnthropicConverter::toRequest(msgs, "model", 4096);

    EXPECT_TRUE(request[QStringLiteral("stream")].toBool());
}

TEST_F(AnthropicConverterTest, ToRequestWithToolDefs)
{
    auto msgs = makeConversation();
    QJsonObject toolDef;
    toolDef[QStringLiteral("name")] = QStringLiteral("read_file");
    toolDef[QStringLiteral("description")] = QStringLiteral("Read a file");
    toolDef[QStringLiteral("input_schema")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")}
    };

    auto request = AnthropicConverter::toRequest(msgs, "model", 4096, {toolDef});

    auto tools = request[QStringLiteral("tools")].toArray();
    ASSERT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0][QStringLiteral("name")].toString(), QStringLiteral("read_file"));
}

TEST_F(AnthropicConverterTest, ToolResultMessageConverted)
{
    QList<act::core::LLMMessage> msgs;

    // Assistant message with tool call
    act::core::LLMMessage assistant;
    assistant.role = act::core::MessageRole::Assistant;
    assistant.content = QStringLiteral("Let me read that file.");
    assistant.toolCall.id = QStringLiteral("call_123");
    assistant.toolCall.name = QStringLiteral("read_file");
    assistant.toolCall.params = QJsonObject{{QStringLiteral("path"), QStringLiteral("src/main.cpp")}};
    msgs.append(assistant);

    // Tool result
    act::core::LLMMessage toolResult;
    toolResult.role = act::core::MessageRole::Tool;
    toolResult.toolCallId = QStringLiteral("call_123");
    toolResult.content = QStringLiteral("// file contents here");
    msgs.append(toolResult);

    auto messages = AnthropicConverter::buildMessages(msgs);

    ASSERT_EQ(messages.size(), 2);

    // Assistant should have tool_use content block
    auto assistantMsg = messages[0].toObject();
    EXPECT_EQ(assistantMsg[QStringLiteral("role")].toString(),
              QStringLiteral("assistant"));
    auto content = assistantMsg[QStringLiteral("content")].toArray();
    ASSERT_GE(content.size(), 2); // text + tool_use

    // Tool result should be converted to user with tool_result
    auto toolMsg = messages[1].toObject();
    EXPECT_EQ(toolMsg[QStringLiteral("role")].toString(),
              QStringLiteral("user"));
    auto toolContent = toolMsg[QStringLiteral("content")].toArray();
    ASSERT_EQ(toolContent.size(), 1);
    EXPECT_EQ(toolContent[0].toObject()[QStringLiteral("type")].toString(),
              QStringLiteral("tool_result"));
}

TEST_F(AnthropicConverterTest, ToolToDefinition)
{
    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    QJsonObject props;
    props[QStringLiteral("path")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("description"), QStringLiteral("File path")}
    };
    schema[QStringLiteral("properties")] = props;
    schema[QStringLiteral("required")] = QJsonArray{QStringLiteral("path")};

    auto def = AnthropicConverter::toolToDefinition(
        QStringLiteral("read_file"),
        QStringLiteral("Read a file from disk"),
        schema);

    EXPECT_EQ(def[QStringLiteral("name")].toString(), QStringLiteral("read_file"));
    EXPECT_EQ(def[QStringLiteral("description")].toString(), QStringLiteral("Read a file from disk"));
    EXPECT_TRUE(def.contains(QStringLiteral("input_schema")));
}

TEST_F(AnthropicConverterTest, AuthHeaders)
{
    auto headers = AnthropicConverter::authHeaders("sk-test-key");

    EXPECT_EQ(headers.value(QStringLiteral("x-api-key")), QStringLiteral("sk-test-key"));
    EXPECT_EQ(headers.value(QStringLiteral("anthropic-version")), QStringLiteral("2023-06-01"));
    EXPECT_EQ(headers.value(QStringLiteral("content-type")), QStringLiteral("application/json"));
}

TEST_F(AnthropicConverterTest, ParseMessageStartEvent)
{
    QJsonObject data;
    data[QStringLiteral("type")] = QStringLiteral("message_start");
    QJsonObject message;
    message[QStringLiteral("stop_reason")] = QStringLiteral("end_turn");
    data[QStringLiteral("message")] = message;

    auto parsed = AnthropicConverter::parseSseEvent("message_start", data);
    EXPECT_EQ(parsed.stopReason, QStringLiteral("end_turn"));
}

TEST_F(AnthropicConverterTest, ParseContentBlockDeltaText)
{
    QJsonObject data;
    data[QStringLiteral("type")] = QStringLiteral("content_block_delta");
    QJsonObject delta;
    delta[QStringLiteral("type")] = QStringLiteral("text_delta");
    delta[QStringLiteral("text")] = QStringLiteral("Hello");
    data[QStringLiteral("delta")] = delta;

    auto parsed = AnthropicConverter::parseSseEvent("content_block_delta", data);
    EXPECT_EQ(parsed.text, QStringLiteral("Hello"));
}

TEST_F(AnthropicConverterTest, ParseContentBlockStartToolUse)
{
    QJsonObject data;
    data[QStringLiteral("type")] = QStringLiteral("content_block_start");
    QJsonObject contentBlock;
    contentBlock[QStringLiteral("type")] = QStringLiteral("tool_use");
    contentBlock[QStringLiteral("id")] = QStringLiteral("call_abc");
    contentBlock[QStringLiteral("name")] = QStringLiteral("read_file");
    data[QStringLiteral("content_block")] = contentBlock;

    auto parsed = AnthropicConverter::parseSseEvent("content_block_start", data);
    ASSERT_EQ(parsed.toolCalls.size(), 1);
    EXPECT_EQ(parsed.toolCalls[0].id, QStringLiteral("call_abc"));
    EXPECT_EQ(parsed.toolCalls[0].name, QStringLiteral("read_file"));
}

TEST_F(AnthropicConverterTest, FinalizeAddsStopReason)
{
    AnthropicConverter::ParsedResponse accumulated;
    accumulated.text = QStringLiteral("Hello");
    // No stop reason set

    auto finalized = AnthropicConverter::finalize(accumulated);
    EXPECT_EQ(finalized.stopReason, QStringLiteral("end_turn"));
    EXPECT_EQ(finalized.text, QStringLiteral("Hello"));
}
