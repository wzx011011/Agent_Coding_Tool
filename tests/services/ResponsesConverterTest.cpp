#include "services/responses_converter.h"
#include <QJsonArray>
#include <gtest/gtest.h>

using namespace act::services;

class ResponsesConverterTest : public ::testing::Test {
  protected:
    static QList<act::core::LLMMessage> makeConversation() {
        QList<act::core::LLMMessage> messages;

        act::core::LLMMessage system;
        system.role = act::core::MessageRole::System;
        system.content = QStringLiteral("You are a helpful assistant.");
        messages.append(system);

        act::core::LLMMessage user;
        user.role = act::core::MessageRole::User;
        user.content = QStringLiteral("List the files in src.");
        messages.append(user);

        return messages;
    }
};

TEST_F(ResponsesConverterTest, ToRequestMovesSystemPromptToInstructions) {
    const auto request = ResponsesConverter::toRequest(makeConversation(), QStringLiteral("gpt-5.4"));

    EXPECT_EQ(request[QStringLiteral("instructions")].toString(), QStringLiteral("You are a helpful assistant."));

    const auto input = request[QStringLiteral("input")].toArray();
    ASSERT_EQ(input.size(), 1);
    EXPECT_EQ(input[0].toObject()[QStringLiteral("role")].toString(), QStringLiteral("user"));
}

TEST_F(ResponsesConverterTest, ToRequestConvertsToolOutputs) {
    QList<act::core::LLMMessage> messages;

    act::core::LLMMessage assistant;
    assistant.role = act::core::MessageRole::Assistant;
    assistant.toolCall.id = QStringLiteral("call_123");
    assistant.toolCall.name = QStringLiteral("read_file");
    assistant.toolCall.params = QJsonObject{{QStringLiteral("path"), QStringLiteral("src/main.cpp")}};
    messages.append(assistant);

    act::core::LLMMessage tool;
    tool.role = act::core::MessageRole::Tool;
    tool.toolCallId = QStringLiteral("call_123");
    tool.content = QStringLiteral("int main() {}");
    messages.append(tool);

    const auto request = ResponsesConverter::toRequest(messages, QStringLiteral("gpt-5.4"));
    const auto input = request[QStringLiteral("input")].toArray();

    ASSERT_EQ(input.size(), 2);
    EXPECT_EQ(input[0].toObject()[QStringLiteral("type")].toString(), QStringLiteral("function_call"));
    EXPECT_EQ(input[1].toObject()[QStringLiteral("type")].toString(), QStringLiteral("function_call_output"));

    const auto output = input[1].toObject()[QStringLiteral("output")].toArray();
    ASSERT_EQ(output.size(), 1);
    EXPECT_EQ(output[0].toObject()[QStringLiteral("type")].toString(), QStringLiteral("input_text"));
}

TEST_F(ResponsesConverterTest, ParseSseTextDelta) {
    act::infrastructure::SseEvent event;
    event.data = QStringLiteral("{\"type\":\"response.output_text.delta\",\"delta\":\"Hello\"}");

    ResponsesConverter::ParsedResponse parsed;
    EXPECT_TRUE(ResponsesConverter::parseSseEvent(event, parsed));
    EXPECT_EQ(parsed.text, QStringLiteral("Hello"));
}

TEST_F(ResponsesConverterTest, ParseSseOutputItemDoneBuildsToolCall) {
    act::infrastructure::SseEvent event;
    event.data =
        QStringLiteral("{\"type\":\"response.output_item.done\",\"item\":{\"type\":\"function_call\",\"call_id\":"
                       "\"call_123\",\"name\":\"read_file\",\"arguments\":\"{\\\"path\\\":\\\"src/main.cpp\\\"}\"}}");

    ResponsesConverter::ParsedResponse parsed;
    EXPECT_TRUE(ResponsesConverter::parseSseEvent(event, parsed));
    ASSERT_EQ(parsed.toolCalls.size(), 1);
    EXPECT_EQ(parsed.toolCalls[0].id, QStringLiteral("call_123"));
    EXPECT_EQ(parsed.toolCalls[0].name, QStringLiteral("read_file"));
    EXPECT_EQ(parsed.toolCalls[0].params[QStringLiteral("path")].toString(), QStringLiteral("src/main.cpp"));
}

TEST_F(ResponsesConverterTest, ParseResponseBodyExtractsTextAndToolCalls) {
    const QByteArray body = R"({
        "status": "completed",
        "output": [
            {
                "type": "message",
                "role": "assistant",
                "content": [
                    {"type": "output_text", "text": "Done."}
                ]
            },
            {
                "type": "function_call",
                "call_id": "call_456",
                "name": "grep",
                "arguments": "{\"pattern\":\"TODO\"}"
            }
        ]
    })";

    const auto parsed = ResponsesConverter::parseResponseBody(body);
    EXPECT_EQ(parsed.status, QStringLiteral("completed"));
    EXPECT_EQ(parsed.text, QStringLiteral("Done."));
    ASSERT_EQ(parsed.toolCalls.size(), 1);
    EXPECT_EQ(parsed.toolCalls[0].name, QStringLiteral("grep"));
    EXPECT_EQ(parsed.toolCalls[0].params[QStringLiteral("pattern")].toString(), QStringLiteral("TODO"));
}