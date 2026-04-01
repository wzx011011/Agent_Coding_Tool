#include <gtest/gtest.h>

#include <QJsonArray>

#include "harness/tools/web_fetch_tool.h"
#include "infrastructure/http_network.h"

using namespace act::harness;

// Minimal test suite for WebFetchTool.
// Network-dependent tests are avoided to keep tests fast and hermetic.

TEST(WebFetchToolTest, NameIsWebFetch)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);
    EXPECT_EQ(tool.name().toStdString(), "web_fetch");
}

TEST(WebFetchToolTest, DescriptionIsNotEmpty)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST(WebFetchToolTest, PermissionLevelIsNetwork)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);
    EXPECT_EQ(tool.permissionLevel(), act::core::PermissionLevel::Network);
}

TEST(WebFetchToolTest, IsNotThreadSafe)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);
    EXPECT_FALSE(tool.isThreadSafe());
}

TEST(WebFetchToolTest, SchemaHasRequiredUrl)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);
    auto schema = tool.schema();

    ASSERT_TRUE(schema.contains(QStringLiteral("required")));
    auto required = schema.value(QStringLiteral("required")).toArray();
    ASSERT_EQ(required.size(), 1);
    EXPECT_EQ(required.at(0).toString().toStdString(), "url");
}

TEST(WebFetchToolTest, SchemaHasUrlProperty)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);
    auto schema = tool.schema();

    ASSERT_TRUE(schema.contains(QStringLiteral("properties")));
    auto props = schema.value(QStringLiteral("properties")).toObject();
    ASSERT_TRUE(props.contains(QStringLiteral("url")));
    EXPECT_EQ(props[QStringLiteral("url")]
                  [QStringLiteral("type")]
                  .toString()
                  .toStdString(),
              "string");
}

TEST(WebFetchToolTest, SchemaHasOptionalHeadersProperty)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);
    auto schema = tool.schema();

    auto props = schema.value(QStringLiteral("properties")).toObject();
    ASSERT_TRUE(props.contains(QStringLiteral("headers")));
    EXPECT_EQ(props[QStringLiteral("headers")]
                  [QStringLiteral("type")]
                  .toString()
                  .toStdString(),
              "object");
}

TEST(WebFetchToolTest, ExecuteRejectsMissingUrl)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);

    QJsonObject emptyParams;
    auto result = tool.execute(emptyParams);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorCode.isEmpty());
}

TEST(WebFetchToolTest, ExecuteRejectsEmptyUrl)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);

    QJsonObject params;
    params[QStringLiteral("url")] = QStringLiteral("");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorCode.isEmpty());
}

TEST(WebFetchToolTest, ExecuteRejectsInvalidUrlType)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);

    QJsonObject params;
    params[QStringLiteral("url")] = 12345;
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorCode.isEmpty());
}

// SSRF protection tests

TEST(WebFetchToolTest, RejectsFtpScheme)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);

    QJsonObject params;
    params[QStringLiteral("url")] = QStringLiteral("ftp://example.com/file");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.contains(QLatin1String("http or https")));
}

TEST(WebFetchToolTest, RejectsFileScheme)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);

    QJsonObject params;
    params[QStringLiteral("url")] = QStringLiteral("file:///etc/passwd");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
}

TEST(WebFetchToolTest, RejectsLoopbackAddress)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);

    QJsonObject params;
    params[QStringLiteral("url")] = QStringLiteral("http://127.0.0.1:8080");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.contains(QLatin1String("private")));
}

TEST(WebFetchToolTest, RejectsCloudMetadataEndpoint)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);

    QJsonObject params;
    params[QStringLiteral("url")] =
        QStringLiteral("http://169.254.169.254/latest/meta-data/");
    auto result = tool.execute(params);

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.contains(QLatin1String("private")));
}

TEST(WebFetchToolTest, RejectsRfc1918Address)
{
    act::infrastructure::HttpNetwork http;
    WebFetchTool tool(http);

    // Test 10.x.x.x
    QJsonObject params10;
    params10[QStringLiteral("url")] =
        QStringLiteral("http://10.0.0.1/admin");
    auto result10 = tool.execute(params10);
    EXPECT_FALSE(result10.success);

    // Test 192.168.x.x
    QJsonObject params192;
    params192[QStringLiteral("url")] =
        QStringLiteral("http://192.168.1.1/api");
    auto result192 = tool.execute(params192);
    EXPECT_FALSE(result192.success);
}

TEST(WebFetchToolTest, MaxResponseSizeIs50KB)
{
    static_assert(WebFetchTool::MAX_RESPONSE_SIZE == 50 * 1024,
                  "MAX_RESPONSE_SIZE should be 50KB");
}

TEST(WebFetchToolTest, FetchTimeoutIs15Seconds)
{
    static_assert(WebFetchTool::FETCH_TIMEOUT_SECONDS == 15,
                  "FETCH_TIMEOUT_SECONDS should be 15");
}
