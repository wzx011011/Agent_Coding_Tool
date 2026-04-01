#include <gtest/gtest.h>

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include "core/enums.h"
#include "core/types.h"
#include "framework/session_serializer.h"

using namespace act::framework;
using namespace act::core;

namespace
{

LLMMessage makeMessage(MessageRole role, const QString &content)
{
    return {.role = role, .content = content};
}

LLMMessage makeToolCallMessage(const QString &content,
                               const QList<ToolCall> &calls)
{
    LLMMessage msg;
    msg.role = MessageRole::Assistant;
    msg.content = content;
    msg.toolCalls = calls;
    if (calls.size() == 1)
    {
        msg.toolCall = calls.first();
    }
    return msg;
}

} // anonymous namespace

class SessionSerializerTest : public ::testing::Test
{
protected:
    QTemporaryDir m_tmpDir;
};

// --- toJson / fromJson round-trip ---

TEST_F(SessionSerializerTest, RoundTripEmptyMessages)
{
    const QList<LLMMessage> messages;
    const QJsonObject json = SessionSerializer::toJson(messages);
    const QList<LLMMessage> restored = SessionSerializer::fromJson(json);

    EXPECT_TRUE(restored.isEmpty());
}

TEST_F(SessionSerializerTest, RoundTripBasicMessages)
{
    QList<LLMMessage> messages;
    messages.append(makeMessage(MessageRole::System, "You are helpful."));
    messages.append(makeMessage(MessageRole::User, "Hello"));
    messages.append(makeMessage(MessageRole::Assistant, "Hi there!"));

    const QJsonObject json = SessionSerializer::toJson(messages);
    const QList<LLMMessage> restored = SessionSerializer::fromJson(json);

    ASSERT_EQ(restored.size(), 3);
    EXPECT_EQ(restored[0].role, MessageRole::System);
    EXPECT_EQ(restored[0].content, QStringLiteral("You are helpful."));
    EXPECT_EQ(restored[1].role, MessageRole::User);
    EXPECT_EQ(restored[1].content, QStringLiteral("Hello"));
    EXPECT_EQ(restored[2].role, MessageRole::Assistant);
    EXPECT_EQ(restored[2].content, QStringLiteral("Hi there!"));
}

TEST_F(SessionSerializerTest, RoundTripToolMessage)
{
    LLMMessage toolMsg;
    toolMsg.role = MessageRole::Tool;
    toolMsg.content = "file contents here";
    toolMsg.toolCallId = "call_123";

    const QList<LLMMessage> messages = {toolMsg};
    const QJsonObject json = SessionSerializer::toJson(messages);
    const QList<LLMMessage> restored = SessionSerializer::fromJson(json);

    ASSERT_EQ(restored.size(), 1);
    EXPECT_EQ(restored[0].role, MessageRole::Tool);
    EXPECT_EQ(restored[0].content, QStringLiteral("file contents here"));
    EXPECT_EQ(restored[0].toolCallId, QStringLiteral("call_123"));
}

TEST_F(SessionSerializerTest, RoundTripAssistantWithToolCalls)
{
    ToolCall tc1;
    tc1.id = "call_1";
    tc1.name = "file_read";
    tc1.params = QJsonObject{{"path", "/tmp/test.txt"}};

    ToolCall tc2;
    tc2.id = "call_2";
    tc2.name = "grep";
    tc2.params = QJsonObject{{"pattern", "TODO"}};

    QList<ToolCall> calls = {tc1, tc2};
    const QList<LLMMessage> messages = {
        makeToolCallMessage("Let me look at the files.", calls)};

    const QJsonObject json = SessionSerializer::toJson(messages);
    const QList<LLMMessage> restored = SessionSerializer::fromJson(json);

    ASSERT_EQ(restored.size(), 1);
    ASSERT_EQ(restored[0].toolCalls.size(), 2);
    EXPECT_EQ(restored[0].toolCalls[0].id, QStringLiteral("call_1"));
    EXPECT_EQ(restored[0].toolCalls[0].name, QStringLiteral("file_read"));
    EXPECT_EQ(restored[0].toolCalls[1].id, QStringLiteral("call_2"));
    EXPECT_EQ(restored[0].toolCalls[1].name, QStringLiteral("grep"));
}

// --- Metadata ---

TEST_F(SessionSerializerTest, MetadataIncludedInJson)
{
    SessionMetadata meta;
    meta.model = "gpt-4";
    meta.provider = "openai";
    meta.totalTokens = 1500;
    meta.inputTokens = 1000;
    meta.outputTokens = 500;
    meta.durationMs = 12345;

    const QJsonObject json = SessionSerializer::toJson({}, meta);

    ASSERT_TRUE(json.contains(QStringLiteral("metadata")));
    const QJsonObject metaObj = json[QStringLiteral("metadata")].toObject();
    EXPECT_EQ(metaObj[QStringLiteral("model")].toString(),
              QStringLiteral("gpt-4"));
    EXPECT_EQ(metaObj[QStringLiteral("provider")].toString(),
              QStringLiteral("openai"));
    EXPECT_EQ(metaObj[QStringLiteral("totalTokens")].toInt(), 1500);
    EXPECT_EQ(metaObj[QStringLiteral("inputTokens")].toInt(), 1000);
    EXPECT_EQ(metaObj[QStringLiteral("outputTokens")].toInt(), 500);
    EXPECT_EQ(metaObj[QStringLiteral("durationMs")].toInt(), 12345);
    EXPECT_FALSE(
        metaObj[QStringLiteral("exportedAt")].toString().isEmpty());
}

TEST_F(SessionSerializerTest, DefaultExportedAtIsCurrentUtc)
{
    const QJsonObject json = SessionSerializer::toJson({});
    const QJsonObject metaObj = json[QStringLiteral("metadata")].toObject();
    const QString exportedAt =
        metaObj[QStringLiteral("exportedAt")].toString();

    // Should be a valid ISO date
    const QDateTime parsed = QDateTime::fromString(exportedAt, Qt::ISODate);
    EXPECT_TRUE(parsed.isValid());
}

// --- validateFormat ---

TEST_F(SessionSerializerTest, ValidateFormatCorrect)
{
    const QJsonObject json = SessionSerializer::toJson({});
    EXPECT_TRUE(SessionSerializer::validateFormat(json));
}

TEST_F(SessionSerializerTest, ValidateFormatWrongIdentifier)
{
    QJsonObject json;
    json[QStringLiteral("format")] = "wrong-format";
    json[QStringLiteral("messages")] = QJsonArray();

    QString error;
    EXPECT_FALSE(SessionSerializer::validateFormat(json, &error));
    EXPECT_TRUE(error.contains(QStringLiteral("Unsupported format")));
}

TEST_F(SessionSerializerTest, ValidateFormatMissingMessages)
{
    QJsonObject json;
    json[QStringLiteral("format")] = "act-session-v1";

    QString error;
    EXPECT_FALSE(SessionSerializer::validateFormat(json, &error));
    EXPECT_TRUE(error.contains(QStringLiteral("messages")));
}

TEST_F(SessionSerializerTest, ValidateFormatMessagesNotArray)
{
    QJsonObject json;
    json[QStringLiteral("format")] = "act-session-v1";
    json[QStringLiteral("messages")] = QStringLiteral("not-array");

    QString error;
    EXPECT_FALSE(SessionSerializer::validateFormat(json, &error));
    EXPECT_TRUE(error.contains(QStringLiteral("array")));
}

// --- saveToFile / loadFromFile ---

TEST_F(SessionSerializerTest, SaveAndLoadFile)
{
    const QString path = m_tmpDir.path() + QStringLiteral("/session.json");

    QList<LLMMessage> messages;
    messages.append(makeMessage(MessageRole::User, "test message"));

    SessionMetadata meta;
    meta.model = "test-model";

    ASSERT_TRUE(SessionSerializer::saveToFile(path, messages, meta));

    const QJsonObject loaded = SessionSerializer::loadFromFile(path);
    EXPECT_FALSE(loaded.isEmpty());

    EXPECT_TRUE(SessionSerializer::validateFormat(loaded));

    const QList<LLMMessage> restored = SessionSerializer::fromJson(loaded);
    ASSERT_EQ(restored.size(), 1);
    EXPECT_EQ(restored[0].content, QStringLiteral("test message"));
}

TEST_F(SessionSerializerTest, SaveToInvalidPathFails)
{
    const QString path = QStringLiteral("/nonexistent/dir/file.json");
    EXPECT_FALSE(
        SessionSerializer::saveToFile(path, {makeMessage(MessageRole::User, "x")}));
}

TEST_F(SessionSerializerTest, LoadFromNonexistentFileReturnsEmpty)
{
    const QJsonObject result =
        SessionSerializer::loadFromFile(QStringLiteral("/nonexistent/file.json"));
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(SessionSerializerTest, LoadInvalidJsonReturnsEmpty)
{
    const QString path = m_tmpDir.path() + QStringLiteral("/bad.json");

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("not valid json {{{");
    file.close();

    const QJsonObject result = SessionSerializer::loadFromFile(path);
    EXPECT_TRUE(result.isEmpty());
}

// --- toMarkdown ---

TEST_F(SessionSerializerTest, MarkdownExportContainsAllRoles)
{
    QList<LLMMessage> messages;
    messages.append(makeMessage(MessageRole::System, "You are helpful."));
    messages.append(makeMessage(MessageRole::User, "What is 2+2?"));
    messages.append(makeMessage(MessageRole::Assistant, "4"));

    const QString md = SessionSerializer::toMarkdown(messages);

    EXPECT_TRUE(md.contains(QStringLiteral("System")));
    EXPECT_TRUE(md.contains(QStringLiteral("User")));
    EXPECT_TRUE(md.contains(QStringLiteral("Assistant")));
    EXPECT_TRUE(md.contains(QStringLiteral("What is 2+2?")));
    EXPECT_TRUE(md.contains(QStringLiteral("You are helpful.")));
    EXPECT_TRUE(md.contains(QStringLiteral("4")));
}

TEST_F(SessionSerializerTest, MarkdownExportIncludesMetadata)
{
    SessionMetadata meta;
    meta.model = "gpt-4";
    meta.provider = "openai";
    meta.totalTokens = 100;

    const QString md = SessionSerializer::toMarkdown({}, meta);

    EXPECT_TRUE(md.contains(QStringLiteral("gpt-4")));
    EXPECT_TRUE(md.contains(QStringLiteral("openai")));
    EXPECT_TRUE(md.contains(QStringLiteral("100")));
}

TEST_F(SessionSerializerTest, MarkdownExportShowsToolCalls)
{
    ToolCall tc;
    tc.id = "call_42";
    tc.name = "file_read";

    const QList<LLMMessage> messages = {
        makeToolCallMessage("Reading files", {tc})};

    const QString md = SessionSerializer::toMarkdown(messages);

    EXPECT_TRUE(md.contains(QStringLiteral("Tool Calls")));
    EXPECT_TRUE(md.contains(QStringLiteral("file_read")));
    EXPECT_TRUE(md.contains(QStringLiteral("call_42")));
}

// --- Format identifier ---

TEST_F(SessionSerializerTest, FormatIdentifierIsCorrect)
{
    const QJsonObject json = SessionSerializer::toJson({});
    EXPECT_EQ(json[QStringLiteral("format")].toString(),
              QStringLiteral("act-session-v1"));
}
