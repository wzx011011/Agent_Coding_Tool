#include <gtest/gtest.h>

#include "infrastructure/sse_parser.h"

using namespace act::infrastructure;

// --- Standard SSE tests ---

TEST(SseParserTest, StandardSingleEvent)
{
    SseParser parser;
    auto events = parser.feed("data: hello\n\n");
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].data, QStringLiteral("hello"));
    EXPECT_TRUE(events[0].eventType.isEmpty());
}

TEST(SseParserTest, StandardDoneMarker)
{
    SseParser parser;
    auto events = parser.feed("data: [DONE]\n\n");
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].data, QStringLiteral("[DONE]"));
}

TEST(SseParserTest, MultiLineData)
{
    SseParser parser;
    auto events = parser.feed("data: line1\ndata: line2\n\n");
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].data, QStringLiteral("line1\nline2"));
}

TEST(SseParserTest, MultipleEvents)
{
    SseParser parser;
    auto events = parser.feed("data: first\n\ndata: second\n\n");
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0].data, QStringLiteral("first"));
    EXPECT_EQ(events[1].data, QStringLiteral("second"));
}

// --- Anthropic SSE tests ---

TEST(SseParserTest, AnthropicEventWithEventType)
{
    SseParser parser;
    auto events = parser.feed(
        "event: message_start\n"
        "data: {\"type\":\"message_start\"}\n\n");
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].eventType, QStringLiteral("message_start"));
    EXPECT_EQ(events[0].data, QStringLiteral("{\"type\":\"message_start\"}"));
}

TEST(SseParserTest, AnthropicContentBlockDelta)
{
    SseParser parser;
    auto events = parser.feed(
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"Hi\"}}\n\n");
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].eventType, QStringLiteral("content_block_delta"));
    EXPECT_FALSE(events[0].data.isEmpty());
}

TEST(SseParserTest, AnthropicMultipleEvents)
{
    SseParser parser;
    auto events = parser.feed(
        "event: message_start\n"
        "data: {\"type\":\"message_start\"}\n\n"
        "event: content_block_delta\n"
        "data: {\"delta\":{\"text\":\"Hello\"}}\n\n"
        "event: message_stop\n"
        "data: {\"type\":\"message_stop\"}\n\n");
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].eventType, QStringLiteral("message_start"));
    EXPECT_EQ(events[1].eventType, QStringLiteral("content_block_delta"));
    EXPECT_EQ(events[2].eventType, QStringLiteral("message_stop"));
}

// --- Chunked input tests ---

TEST(SseParserTest, ChunkedInput)
{
    SseParser parser;
    auto e1 = parser.feed("data: he");
    EXPECT_TRUE(e1.isEmpty());

    auto e2 = parser.feed("llo\n\n");
    ASSERT_EQ(e2.size(), 1);
    EXPECT_EQ(e2[0].data, QStringLiteral("hello"));
}

TEST(SseParserTest, ChunkedAcrossMultipleEvents)
{
    SseParser parser;
    auto e1 = parser.feed("data: first\n\ndata: sec");
    ASSERT_EQ(e1.size(), 1);
    EXPECT_EQ(e1[0].data, QStringLiteral("first"));

    auto e2 = parser.feed("ond\n\n");
    ASSERT_EQ(e2.size(), 1);
    EXPECT_EQ(e2[0].data, QStringLiteral("second"));
}

// --- Edge cases ---

TEST(SseParserTest, CommentLinesIgnored)
{
    SseParser parser;
    auto events = parser.feed(": this is a comment\ndata: hello\n\n");
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].data, QStringLiteral("hello"));
}

TEST(SseParserTest, EmptyFeed)
{
    SseParser parser;
    auto events = parser.feed("");
    EXPECT_TRUE(events.isEmpty());
}

TEST(SseParserTest, FlushIncompleteEvent)
{
    SseParser parser;
    (void)parser.feed("data: partial");
    auto events = parser.flush();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].data, QStringLiteral("partial"));
}

TEST(SseParserTest, Reset)
{
    SseParser parser;
    (void)parser.feed("data: partial");
    parser.reset();
    auto events = parser.feed("data: new\n\n");
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].data, QStringLiteral("new"));
}

TEST(SseParserTest, EventIdTracked)
{
    SseParser parser;
    (void)parser.feed("id: evt-123\ndata: test\n\n");
    EXPECT_EQ(parser.lastEventId(), QStringLiteral("evt-123"));
}

TEST(SseParserTest, ColonAfterFieldNameRemoved)
{
    SseParser parser;
    auto events = parser.feed("data: hello\n\n");
    ASSERT_EQ(events.size(), 1);
    // The space after colon should be trimmed
    EXPECT_EQ(events[0].data, QStringLiteral("hello"));
}

TEST(SseParserTest, CRLFLineEnding)
{
    SseParser parser;
    auto events = parser.feed("data: hello\r\n\r\n");
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].data, QStringLiteral("hello"));
}

TEST(SseParserTest, MixedRealWorldAnthropicStream)
{
    SseParser parser;
    // Simulate a real Anthropic SSE stream
    QByteArray stream =
        "event: message_start\n"
        "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_123\",\"role\":\"assistant\"}}\n"
        "\n"
        "event: content_block_start\n"
        "data: {\"type\":\"content_block_start\",\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n"
        "\n"
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}\n"
        "\n"
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\" world\"}}\n"
        "\n"
        "event: content_block_stop\n"
        "data: {\"type\":\"content_block_stop\"}\n"
        "\n"
        "event: message_delta\n"
        "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"}}\n"
        "\n"
        "event: message_stop\n"
        "data: {\"type\":\"message_stop\"}\n"
        "\n";

    auto events = parser.feed(stream);
    ASSERT_EQ(events.size(), 7);

    // First event: message_start
    EXPECT_EQ(events[0].eventType, QStringLiteral("message_start"));

    // Second: content_block_start
    EXPECT_EQ(events[1].eventType, QStringLiteral("content_block_start"));

    // Third: first text delta
    EXPECT_EQ(events[2].eventType, QStringLiteral("content_block_delta"));

    // Fourth: second text delta
    EXPECT_EQ(events[3].eventType, QStringLiteral("content_block_delta"));

    // Fifth: content_block_stop
    EXPECT_EQ(events[4].eventType, QStringLiteral("content_block_stop"));

    // Sixth: message_delta with stop_reason
    EXPECT_EQ(events[5].eventType, QStringLiteral("message_delta"));

    // Seventh: message_stop
    EXPECT_EQ(events[6].eventType, QStringLiteral("message_stop"));
}
