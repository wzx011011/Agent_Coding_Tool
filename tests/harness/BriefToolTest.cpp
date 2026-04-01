#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/brief_tool.h"
#include "services/interfaces.h"
#include "test_helpers/MockAIEngine.h"

using namespace act::harness;
using namespace act::core;

namespace
{

/// Helper to create long content that exceeds MIN_CONTENT_LENGTH.
QString longContent(int length = 200)
{
    return QString(length, QLatin1Char('x'));
}

} // anonymous namespace

class BriefToolTest : public ::testing::Test
{
protected:
    MockAIEngine m_engine;
};

// --- Validation tests ---

TEST_F(BriefToolTest, NameIsBrief)
{
    BriefTool tool(m_engine);
    EXPECT_EQ(tool.name(), QStringLiteral("brief"));
}

TEST_F(BriefToolTest, PermissionLevelIsRead)
{
    BriefTool tool(m_engine);
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);
}

TEST_F(BriefToolTest, IsNotThreadSafe)
{
    BriefTool tool(m_engine);
    EXPECT_FALSE(tool.isThreadSafe());
}

TEST_F(BriefToolTest, SchemaHasRequiredContent)
{
    BriefTool tool(m_engine);
    const QJsonObject schema = tool.schema();

    EXPECT_EQ(schema[QStringLiteral("type")].toString(),
              QStringLiteral("object"));

    const QJsonArray required =
        schema[QStringLiteral("required")].toArray();
    ASSERT_GE(required.size(), 1);
    EXPECT_TRUE(required.contains(QStringLiteral("content")));

    const QJsonObject props =
        schema[QStringLiteral("properties")].toObject();
    EXPECT_TRUE(props.contains(QStringLiteral("content")));
    EXPECT_TRUE(props.contains(QStringLiteral("max_tokens")));
}

// --- Parameter validation ---

TEST_F(BriefToolTest, MissingContentReturnsError)
{
    BriefTool tool(m_engine);
    const auto result = tool.execute(QJsonObject{});

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, QStringLiteral("INVALID_PARAMS"));
}

TEST_F(BriefToolTest, NonStringContentReturnsError)
{
    BriefTool tool(m_engine);
    const auto result =
        tool.execute(QJsonObject{{QStringLiteral("content"), 123}});

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, QStringLiteral("INVALID_PARAMS"));
}

TEST_F(BriefToolTest, EmptyContentReturnsError)
{
    BriefTool tool(m_engine);
    const auto result = tool.execute(
        QJsonObject{{QStringLiteral("content"), QString()}});

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, QStringLiteral("INVALID_PARAMS"));
}

// --- Short content passthrough ---

TEST_F(BriefToolTest, ShortContentReturnedAsIs)
{
    BriefTool tool(m_engine);
    const QString shortText = QStringLiteral("Hello, this is short.");
    const auto result = tool.execute(
        QJsonObject{{QStringLiteral("content"), shortText}});

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, shortText);
    // Engine should NOT have been called
    EXPECT_EQ(m_engine.callCount, 0);
}

// --- LLM summarization ---

TEST_F(BriefToolTest, LongContentCallsEngine)
{
    BriefTool tool(m_engine);

    // Enqueue a summary response
    LLMMessage response;
    response.role = MessageRole::Assistant;
    response.content = QStringLiteral("Summary of content");
    m_engine.responseQueue.append(response);

    const auto result = tool.execute(
        QJsonObject{{QStringLiteral("content"), longContent()}});

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, QStringLiteral("Summary of content"));
    EXPECT_EQ(m_engine.callCount, 1);
}

TEST_F(BriefToolTest, EngineErrorPropagates)
{
    BriefTool tool(m_engine);

    // Leave responseQueue empty to trigger error
    const auto result = tool.execute(
        QJsonObject{{QStringLiteral("content"), longContent()}});

    EXPECT_FALSE(result.success);
    EXPECT_EQ(m_engine.callCount, 1);
    EXPECT_FALSE(result.error.isEmpty());
}

TEST_F(BriefToolTest, CustomMaxTokensPassedToPrompt)
{
    BriefTool tool(m_engine);

    LLMMessage response;
    response.role = MessageRole::Assistant;
    response.content = QStringLiteral("Brief summary");
    m_engine.responseQueue.append(response);

    const auto result = tool.execute(
        QJsonObject{{QStringLiteral("content"), longContent()},
                    {QStringLiteral("max_tokens"), 100}});

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, QStringLiteral("Brief summary"));
    EXPECT_EQ(m_engine.callCount, 1);
}

// --- Edge cases ---

TEST_F(BriefToolTest, BelowMinContentLengthReturnedAsIs)
{
    BriefTool tool(m_engine);
    // Content below MIN_CONTENT_LENGTH chars should NOT trigger LLM
    const QString shorty =
        QString(BriefTool::MIN_CONTENT_LENGTH - 1, QLatin1Char('a'));
    const auto result = tool.execute(
        QJsonObject{{QStringLiteral("content"), shorty}});

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, shorty);
    EXPECT_EQ(m_engine.callCount, 0);
}

TEST_F(BriefToolTest, AtMinContentLengthTriggersEngine)
{
    BriefTool tool(m_engine);

    LLMMessage response;
    response.role = MessageRole::Assistant;
    response.content = QStringLiteral("summarized");
    m_engine.responseQueue.append(response);

    // Content of exactly MIN_CONTENT_LENGTH chars triggers LLM (not < MIN)
    const QString edge =
        QString(BriefTool::MIN_CONTENT_LENGTH, QLatin1Char('a'));
    const auto result = tool.execute(
        QJsonObject{{QStringLiteral("content"), edge}});

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, QStringLiteral("summarized"));
    EXPECT_EQ(m_engine.callCount, 1);
}
