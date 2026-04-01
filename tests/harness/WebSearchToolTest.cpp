#include <gtest/gtest.h>

#include <QJsonArray>

#include "harness/tools/web_search_tool.h"
#include "infrastructure/http_network.h"

using namespace act::harness;

// Minimal test suite for WebSearchTool.
// Network-dependent tests are avoided to keep tests fast and hermetic.

TEST(WebSearchToolTest, NameIsWebSearch)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);
    EXPECT_EQ(tool.name().toStdString(), "web_search");
}

TEST(WebSearchToolTest, DescriptionIsNotEmpty)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(WebSearchToolTest, PermissionLevelIsNetwork)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);
    EXPECT_EQ(tool.permissionLevel(), act::core::PermissionLevel::Network);
}

TEST(WebSearchToolTest, IsNotThreadSafe)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);
    EXPECT_FALSE(tool.isThreadSafe());
}

TEST(WebSearchToolTest, SchemaHasRequiredQuery)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);
    auto schema = tool.schema();

    ASSERT_TRUE(schema.contains(QStringLiteral("required")));
    auto required = schema.value(QStringLiteral("required")).toArray();
    ASSERT_EQ(required.size(), 1);
    EXPECT_EQ(required.at(0).toString().toStdString(), "query");
}

TEST(WebSearchToolTest, SchemaHasQueryProperty)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);
    auto schema = tool.schema();

    ASSERT_TRUE(schema.contains(QStringLiteral("properties")));
    auto props = schema.value(QStringLiteral("properties")).toObject();
    ASSERT_TRUE(props.contains(QStringLiteral("query")));
    EXPECT_EQ(props[QStringLiteral("query")]
                  [QStringLiteral("type")]
                  .toString()
                  .toStdString(),
              "string");
}

TEST(WebSearchToolTest, SchemaHasMaxResultsProperty)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);
    auto schema = tool.schema();

    auto props = schema.value(QStringLiteral("properties")).toObject();
    ASSERT_TRUE(props.contains(QStringLiteral("max_results")));
    EXPECT_EQ(props[QStringLiteral("max_results")]
                  [QStringLiteral("type")]
                  .toString()
                  .toStdString(),
              "integer");
}

TEST(WebSearchToolTest, ExecuteRejectsMissingQuery)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);

    QJsonObject emptyParams;
    auto result = tool.execute(emptyParams);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorCode.isEmpty());
}

TEST(WebSearchToolTest, ExecuteRejectsEmptyQuery)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);

    QJsonObject params;
    params[QStringLiteral("query")] = QStringLiteral("");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode.toStdString(), "INVALID_PARAMS");
}

TEST(WebSearchToolTest, ExecuteRejectsNonStringQuery)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);

    QJsonObject params;
    params[QStringLiteral("query")] = 12345;
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode.toStdString(), "INVALID_PARAMS");
}

TEST(WebSearchToolTest, ExecuteRejectsWhitespaceOnlyQuery)
{
    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);

    QJsonObject params;
    params[QStringLiteral("query")] = QStringLiteral("   ");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode.toStdString(), "INVALID_PARAMS");
}

TEST(WebSearchToolTest, ExecuteRejectsMissingApiKey)
{
    // Ensure the env var is not set during the test
    auto oldKey = qgetenv("ACT_SEARCH_API_KEY");
    qunsetenv("ACT_SEARCH_API_KEY");

    act::infrastructure::HttpNetwork http;
    WebSearchTool tool(http);

    QJsonObject params;
    params[QStringLiteral("query")] = QStringLiteral("test query");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode.toStdString(), "AUTH_ERROR");
    EXPECT_TRUE(result.error.contains(
        QLatin1String("ACT_SEARCH_API_KEY")));

    // Restore if it was previously set
    if (!oldKey.isEmpty())
    {
        qputenv("ACT_SEARCH_API_KEY", oldKey);
    }
}

TEST(WebSearchToolTest, MaxResultsConstantIs10)
{
    static_assert(WebSearchTool::MAX_RESULTS == 10,
                  "MAX_RESULTS should be 10");
}

TEST(WebSearchToolTest, SearchTimeoutIs15Seconds)
{
    static_assert(WebSearchTool::SEARCH_TIMEOUT_SECONDS == 15,
                  "SEARCH_TIMEOUT_SECONDS should be 15");
}
