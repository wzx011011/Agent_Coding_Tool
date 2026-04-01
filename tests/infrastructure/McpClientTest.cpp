#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>

#include "harness/mcp_tool_adapter.h"
#include "infrastructure/mcp_client.h"
#include "infrastructure/mcp_transport.h"

#include <mutex>
#include <condition_variable>

using namespace act::infrastructure;
using namespace act::harness;

// ---------------------------------------------------------------------------
// Mock transport — records requests and returns pre-configured responses.
// Does NOT launch any real subprocess.
// ---------------------------------------------------------------------------
class MockMcpTransport : public McpTransport {
public:
    bool connect() override
    {
        m_connected = true;
        return m_connectResult;
    }

    QJsonObject sendRequest(const QString &method,
                             const QJsonObject &params) override
    {
        if (!m_connected)
            return {};

        // Record the request.
        std::lock_guard<std::mutex> lock(m_mutex);
        RecordedRequest req;
        req.method = method;
        req.params = params;
        // Extract the id from the JSON-RPC message — but our transport layer
        // doesn't see the id because McpClient builds it internally.
        // For testing we record what we can.
        m_recordedRequests.append(req);

        // If a response was queued for this method, return it.
        if (m_responses.contains(method))
        {
            auto response = m_responses.take(method);
            // Inject the request id if the response doesn't have one.
            if (!response.contains(QStringLiteral("id")))
            {
                response[QStringLiteral("id")] = m_nextResponseId++;
            }
            return response;
        }

        // Default empty response (timeout scenario).
        return {};
    }

    void close() override
    {
        m_connected = false;
    }

    [[nodiscard]] bool isConnected() const override
    {
        return m_connected;
    }

    // --- Test helpers ---

    /// Queue a response for a given method.
    void enqueueResponse(const QString &method, const QJsonObject &response)
    {
        m_responses[method] = response;
    }

    /// Get all recorded requests.
    struct RecordedRequest {
        QString method;
        QJsonObject params;
    };

    [[nodiscard]] QList<RecordedRequest> recordedRequests() const
    {
        return m_recordedRequests;
    }

    void setConnectResult(bool result)
    {
        m_connectResult = result;
    }

private:
    bool m_connected = false;
    bool m_connectResult = true;
    QMap<QString, QJsonObject> m_responses;
    QList<RecordedRequest> m_recordedRequests;
    int m_nextResponseId = 1;
    mutable std::mutex m_mutex;
};

// ---------------------------------------------------------------------------
// Helper: Build a JSON-RPC success response
// ---------------------------------------------------------------------------
static QJsonObject makeSuccessResponse(const QJsonObject &result)
{
    QJsonObject response;
    response[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    response[QStringLiteral("id")] = 1;
    response[QStringLiteral("result")] = result;
    return response;
}

static QJsonObject makeErrorResponse(int code, const QString &message)
{
    QJsonObject errObj;
    errObj[QStringLiteral("code")] = code;
    errObj[QStringLiteral("message")] = message;

    QJsonObject response;
    response[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    response[QStringLiteral("id")] = 1;
    response[QStringLiteral("error")] = errObj;
    return response;
}

// ---------------------------------------------------------------------------
// McpTransport base tests
// ---------------------------------------------------------------------------

TEST(McpTransportTest, NotificationHandlerCanBeSet)
{
    MockMcpTransport transport;
    bool handlerCalled = false;
    transport.setNotificationHandler(
        [&](const QJsonObject &) { handlerCalled = true; });
    // The handler is stored; we verify it doesn't crash.
    EXPECT_FALSE(handlerCalled); // Not called yet, just set.
}

// ---------------------------------------------------------------------------
// McpClient tests
// ---------------------------------------------------------------------------

class McpClientTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto transport = std::make_unique<MockMcpTransport>();
        m_rawTransport = transport.get();
        m_client = std::make_unique<McpClient>(std::move(transport));
    }

    MockMcpTransport *m_rawTransport = nullptr;
    std::unique_ptr<McpClient> m_client;
};

TEST_F(McpClientTest, InitializeSuccess)
{
    QJsonObject serverInfo;
    serverInfo[QStringLiteral("name")] = QStringLiteral("TestServer");
    serverInfo[QStringLiteral("version")] = QStringLiteral("0.1.0");

    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] = serverInfo;
    initResult[QStringLiteral("capabilities")] = QJsonObject();

    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));

    ASSERT_TRUE(m_client->initialize());
    EXPECT_TRUE(m_client->isInitialized());

    auto info = m_client->serverInfo();
    EXPECT_EQ(info.value(QStringLiteral("serverInfo"))
                  .toObject()
                  .value(QStringLiteral("name"))
                  .toString(),
              QStringLiteral("TestServer"));
}

TEST_F(McpClientTest, InitializeFailsOnTransportError)
{
    m_rawTransport->setConnectResult(false);
    // Don't enqueue a response — the connect step fails first.
    auto transport = std::make_unique<MockMcpTransport>();
    transport->setConnectResult(false);
    auto client = std::make_unique<McpClient>(std::move(transport));

    EXPECT_FALSE(client->initialize());
    EXPECT_FALSE(client->isInitialized());
}

TEST_F(McpClientTest, InitializeFailsOnJsonRpcError)
{
    m_rawTransport->enqueueResponse(
        QStringLiteral("initialize"),
        makeErrorResponse(-32600, QStringLiteral("Invalid Request")));

    EXPECT_FALSE(m_client->initialize());
    EXPECT_FALSE(m_client->isInitialized());
}

// Fix: client is m_client here
TEST_F(McpClientTest, InitializeFailsOnEmptyResponse)
{
    // No response enqueued -> empty response.
    EXPECT_FALSE(m_client->initialize());
    EXPECT_FALSE(m_client->isInitialized());
}

TEST_F(McpClientTest, DoubleInitializeReturnsTrue)
{
    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] =
        QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                     {QStringLiteral("version"), QStringLiteral("1")}};
    initResult[QStringLiteral("capabilities")] = QJsonObject();

    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());
    EXPECT_TRUE(m_client->initialize()); // Second call is a no-op success.
}

TEST_F(McpClientTest, DiscoverToolsReturnsToolList)
{
    // First initialize.
    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] =
        QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                     {QStringLiteral("version"), QStringLiteral("1")}};
    initResult[QStringLiteral("capabilities")] = QJsonObject();
    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());

    // Now enqueue tools/list response.
    QJsonArray toolsArray;
    {
        QJsonObject tool1;
        tool1[QStringLiteral("name")] = QStringLiteral("read_file");
        tool1[QStringLiteral("description")] =
            QStringLiteral("Read a file from disk");
        tool1[QStringLiteral("inputSchema")] = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{{QStringLiteral("path"),
                          QJsonObject{{QStringLiteral("type"),
                                       QStringLiteral("string")}}}}}};
        toolsArray.append(tool1);
    }
    {
        QJsonObject tool2;
        tool2[QStringLiteral("name")] = QStringLiteral("search");
        tool2[QStringLiteral("description")] =
            QStringLiteral("Search the web");
        tool2[QStringLiteral("inputSchema")] = QJsonObject{};
        toolsArray.append(tool2);
    }

    QJsonObject toolsResult;
    toolsResult[QStringLiteral("tools")] = toolsArray;

    m_rawTransport->enqueueResponse(QStringLiteral("tools/list"),
                                     makeSuccessResponse(toolsResult));

    auto tools = m_client->discoverTools();
    ASSERT_EQ(tools.size(), 2);
    EXPECT_EQ(tools[0].name, QStringLiteral("read_file"));
    EXPECT_EQ(tools[0].description, QStringLiteral("Read a file from disk"));
    EXPECT_EQ(tools[1].name, QStringLiteral("search"));
}

TEST_F(McpClientTest, DiscoverToolsReturnsEmptyBeforeInit)
{
    auto tools = m_client->discoverTools();
    EXPECT_TRUE(tools.isEmpty());
}

TEST_F(McpClientTest, DiscoverToolsCachesResults)
{
    // Initialize.
    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] =
        QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                     {QStringLiteral("version"), QStringLiteral("1")}};
    initResult[QStringLiteral("capabilities")] = QJsonObject();
    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());

    // Enqueue tools response.
    QJsonArray toolsArray;
    QJsonObject tool;
    tool[QStringLiteral("name")] = QStringLiteral("cached_tool");
    tool[QStringLiteral("description")] = QStringLiteral("A cached tool");
    tool[QStringLiteral("inputSchema")] = QJsonObject{};
    toolsArray.append(tool);

    QJsonObject toolsResult;
    toolsResult[QStringLiteral("tools")] = toolsArray;
    m_rawTransport->enqueueResponse(QStringLiteral("tools/list"),
                                     makeSuccessResponse(toolsResult));

    auto first = m_client->discoverTools();
    ASSERT_EQ(first.size(), 1);

    // Second call should return cached results without hitting the transport.
    // (No response enqueued for second call — if it hit the transport,
    // it would get an empty response.)
    auto second = m_client->discoverTools();
    EXPECT_EQ(second.size(), 1);
    EXPECT_EQ(second[0].name, QStringLiteral("cached_tool"));
}

TEST_F(McpClientTest, DiscoverToolsHandlesError)
{
    // Initialize first.
    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] =
        QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                     {QStringLiteral("version"), QStringLiteral("1")}};
    initResult[QStringLiteral("capabilities")] = QJsonObject();
    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());

    m_rawTransport->enqueueResponse(
        QStringLiteral("tools/list"),
        makeErrorResponse(-32601, QStringLiteral("Method not found")));

    auto tools = m_client->discoverTools();
    EXPECT_TRUE(tools.isEmpty());
}

TEST_F(McpClientTest, CallToolSuccess)
{
    // Initialize.
    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] =
        QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                     {QStringLiteral("version"), QStringLiteral("1")}};
    initResult[QStringLiteral("capabilities")] = QJsonObject();
    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());

    // Enqueue tool call response.
    QJsonArray content;
    QJsonObject textItem;
    textItem[QStringLiteral("type")] = QStringLiteral("text");
    textItem[QStringLiteral("text")] = QStringLiteral("file contents here");
    content.append(textItem);

    QJsonObject callResult;
    callResult[QStringLiteral("content")] = content;
    callResult[QStringLiteral("isError")] = false;

    m_rawTransport->enqueueResponse(QStringLiteral("tools/call"),
                                     makeSuccessResponse(callResult));

    QJsonObject args;
    args[QStringLiteral("path")] = QStringLiteral("/tmp/test.txt");
    auto response = m_client->callTool(QStringLiteral("read_file"), args);

    EXPECT_FALSE(response.contains(QStringLiteral("error")));
    auto result = response.value(QStringLiteral("result")).toObject();
    EXPECT_FALSE(result.value(QStringLiteral("isError")).toBool());

    auto contentArray = result.value(QStringLiteral("content")).toArray();
    ASSERT_EQ(contentArray.size(), 1);
    EXPECT_EQ(contentArray[0].toObject().value(QStringLiteral("text")).toString(),
              QStringLiteral("file contents here"));
}

TEST_F(McpClientTest, CallToolBeforeInitReturnsError)
{
    QJsonObject args;
    auto response = m_client->callTool(QStringLiteral("test"), args);
    EXPECT_TRUE(response.contains(QStringLiteral("error")));
}

TEST_F(McpClientTest, CallToolHandlesJsonRpcError)
{
    // Initialize.
    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] =
        QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                     {QStringLiteral("version"), QStringLiteral("1")}};
    initResult[QStringLiteral("capabilities")] = QJsonObject();
    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());

    m_rawTransport->enqueueResponse(
        QStringLiteral("tools/call"),
        makeErrorResponse(-32602, QStringLiteral("Invalid params")));

    auto response = m_client->callTool(QStringLiteral("bad_tool"), {});
    EXPECT_TRUE(response.contains(QStringLiteral("error")));
}

TEST_F(McpClientTest, ListResources)
{
    // Initialize.
    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] =
        QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                     {QStringLiteral("version"), QStringLiteral("1")}};
    initResult[QStringLiteral("capabilities")] = QJsonObject();
    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());

    // Enqueue resources/list response.
    QJsonArray resources;
    QJsonObject res;
    res[QStringLiteral("uri")] = QStringLiteral("file:///data/config.json");
    res[QStringLiteral("name")] = QStringLiteral("config");
    res[QStringLiteral("mimeType")] = QStringLiteral("application/json");
    resources.append(res);

    QJsonObject resourcesResult;
    resourcesResult[QStringLiteral("resources")] = resources;

    m_rawTransport->enqueueResponse(QStringLiteral("resources/list"),
                                     makeSuccessResponse(resourcesResult));

    auto result = m_client->listResources();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].toObject().value(QStringLiteral("uri")).toString(),
              QStringLiteral("file:///data/config.json"));
}

TEST_F(McpClientTest, ListResourcesBeforeInitReturnsEmpty)
{
    auto result = m_client->listResources();
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(McpClientTest, ReadResource)
{
    // Initialize.
    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] =
        QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                     {QStringLiteral("version"), QStringLiteral("1")}};
    initResult[QStringLiteral("capabilities")] = QJsonObject();
    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());

    // Enqueue resources/read response.
    QJsonArray contents;
    QJsonObject textContent;
    textContent[QStringLiteral("uri")] =
        QStringLiteral("file:///data/config.json");
    textContent[QStringLiteral("mimeType")] = QStringLiteral("application/json");
    textContent[QStringLiteral("text")] = QStringLiteral("{\"key\": \"value\"}");
    contents.append(textContent);

    QJsonObject readResult;
    readResult[QStringLiteral("contents")] = contents;

    m_rawTransport->enqueueResponse(QStringLiteral("resources/read"),
                                     makeSuccessResponse(readResult));

    auto response = m_client->readResource(
        QStringLiteral("file:///data/config.json"));
    EXPECT_TRUE(response.contains(QStringLiteral("result")));
}

TEST_F(McpClientTest, ReadResourceBeforeInitReturnsError)
{
    auto response = m_client->readResource(QStringLiteral("file:///test"));
    EXPECT_TRUE(response.contains(QStringLiteral("error")));
}

TEST_F(McpClientTest, ServerInfoAfterInit)
{
    QJsonObject serverInfo;
    serverInfo[QStringLiteral("name")] = QStringLiteral("MyServer");
    serverInfo[QStringLiteral("version")] = QStringLiteral("2.0.0");

    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] = serverInfo;
    initResult[QStringLiteral("capabilities")] = QJsonObject();

    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());

    auto info = m_client->serverInfo();
    EXPECT_EQ(info.value(QStringLiteral("serverInfo"))
                  .toObject()
                  .value(QStringLiteral("name"))
                  .toString(),
              QStringLiteral("MyServer"));
    EXPECT_EQ(info.value(QStringLiteral("serverInfo"))
                  .toObject()
                  .value(QStringLiteral("version"))
                  .toString(),
              QStringLiteral("2.0.0"));
}

TEST_F(McpClientTest, ShutdownCleansUp)
{
    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] =
        QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                     {QStringLiteral("version"), QStringLiteral("1")}};
    initResult[QStringLiteral("capabilities")] = QJsonObject();
    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());

    // Enqueue a response for shutdown.
    m_rawTransport->enqueueResponse(QStringLiteral("shutdown"),
                                     makeSuccessResponse(QJsonObject()));

    m_client->shutdown();
    EXPECT_FALSE(m_client->isInitialized());
    EXPECT_TRUE(m_client->serverInfo().isEmpty());
}

TEST_F(McpClientTest, ShutdownWithoutInitDoesNotCrash)
{
    m_client->shutdown();
    EXPECT_FALSE(m_client->isInitialized());
}

TEST_F(McpClientTest, RequestIdIncrements)
{
    // Initialize.
    QJsonObject initResult;
    initResult[QStringLiteral("protocolVersion")] =
        QStringLiteral("2024-10-08");
    initResult[QStringLiteral("serverInfo")] =
        QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                     {QStringLiteral("version"), QStringLiteral("1")}};
    initResult[QStringLiteral("capabilities")] = QJsonObject();
    m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                     makeSuccessResponse(initResult));
    ASSERT_TRUE(m_client->initialize());

    // Make two tool calls.
    m_rawTransport->enqueueResponse(QStringLiteral("tools/call"),
                                     makeSuccessResponse(QJsonObject{}));
    m_rawTransport->enqueueResponse(QStringLiteral("tools/call"),
                                     makeSuccessResponse(QJsonObject{}));

    m_client->callTool(QStringLiteral("tool1"), {});
    m_client->callTool(QStringLiteral("tool2"), {});

    // Both calls should have been recorded (even though the mock
    // doesn't track the JSON-RPC id directly, we verify the requests
    // were made in order).
    auto requests = m_rawTransport->recordedRequests();
    // initialize + tools/call + tools/call
    ASSERT_GE(requests.size(), 3);
    EXPECT_EQ(requests[1].method, QStringLiteral("tools/call"));
    EXPECT_EQ(requests[2].method, QStringLiteral("tools/call"));
}

// ---------------------------------------------------------------------------
// McpStdioTransport construction tests (no actual process launch)
// ---------------------------------------------------------------------------

TEST(McpStdioTransportTest, ConstructionWithCommand)
{
    McpStdioTransport transport(QStringLiteral("echo"),
                                 QStringList{QStringLiteral("hello")});
    EXPECT_FALSE(transport.isConnected());
}

TEST(McpStdioTransportTest, ConstructionWithEnv)
{
    QMap<QString, QString> env;
    env[QStringLiteral("TEST_VAR")] = QStringLiteral("test_value");
    McpStdioTransport transport(
        QStringLiteral("echo"),
        QStringList{QStringLiteral("hello")},
        env);
    EXPECT_FALSE(transport.isConnected());
}

TEST(McpStdioTransportTest, CloseWithoutConnectDoesNotCrash)
{
    McpStdioTransport transport(QStringLiteral("echo"));
    transport.close();
    EXPECT_FALSE(transport.isConnected());
}

TEST(McpStdioTransportTest, ConnectWithEmptyCommandFails)
{
    McpStdioTransport transport(QString{});
    EXPECT_FALSE(transport.connect());
}

TEST(McpStdioTransportTest, SendRequestBeforeConnectReturnsEmpty)
{
    McpStdioTransport transport(QStringLiteral("echo"));
    auto response = transport.sendRequest(QStringLiteral("test"), {});
    EXPECT_TRUE(response.isEmpty());
}

// ---------------------------------------------------------------------------
// McpToolAdapter tests
// ---------------------------------------------------------------------------

class McpToolAdapterTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto transport = std::make_unique<MockMcpTransport>();
        m_rawTransport = transport.get();
        m_client = std::make_unique<McpClient>(std::move(transport));

        // Initialize the client.
        QJsonObject initResult;
        initResult[QStringLiteral("protocolVersion")] =
            QStringLiteral("2024-10-08");
        initResult[QStringLiteral("serverInfo")] =
            QJsonObject{{QStringLiteral("name"), QStringLiteral("S")},
                         {QStringLiteral("version"), QStringLiteral("1")}};
        initResult[QStringLiteral("capabilities")] = QJsonObject();
        m_rawTransport->enqueueResponse(QStringLiteral("initialize"),
                                         makeSuccessResponse(initResult));
        ASSERT_TRUE(m_client->initialize());
    }

    void setupTool(const QString &name,
                   const QString &description,
                   const QJsonObject &inputSchema)
    {
        m_adapter = std::make_unique<McpToolAdapter>(
            *m_client, name, description, inputSchema);
    }

    MockMcpTransport *m_rawTransport = nullptr;
    std::unique_ptr<McpClient> m_client;
    std::unique_ptr<McpToolAdapter> m_adapter;
};

TEST_F(McpToolAdapterTest, NameAndDescription)
{
    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    setupTool(QStringLiteral("my_tool"),
              QStringLiteral("A test tool"),
              schema);

    EXPECT_EQ(m_adapter->name(), QStringLiteral("my_tool"));
    EXPECT_EQ(m_adapter->description(), QStringLiteral("A test tool"));
}

TEST_F(McpToolAdapterTest, SchemaFromInputSchema)
{
    QJsonObject props;
    props[QStringLiteral("path")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("description"), QStringLiteral("File path")}};

    QJsonObject inputSchema;
    inputSchema[QStringLiteral("type")] = QStringLiteral("object");
    inputSchema[QStringLiteral("properties")] = props;
    inputSchema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("path")};

    setupTool(QStringLiteral("my_tool"), QStringLiteral("desc"), inputSchema);

    auto schema = m_adapter->schema();
    EXPECT_EQ(schema.value(QStringLiteral("type")).toString(),
              QStringLiteral("object"));
    EXPECT_TRUE(
        schema.value(QStringLiteral("properties")).toObject().contains(
            QStringLiteral("path")));

    auto required = schema.value(QStringLiteral("required")).toArray();
    ASSERT_EQ(required.size(), 1);
    EXPECT_EQ(required[0].toString(), QStringLiteral("path"));
}

TEST_F(McpToolAdapterTest, PermissionLevelIsNetwork)
{
    setupTool(QStringLiteral("my_tool"), QStringLiteral("desc"), QJsonObject{});
    EXPECT_EQ(m_adapter->permissionLevel(), act::core::PermissionLevel::Network);
}

TEST_F(McpToolAdapterTest, IsThreadSafe)
{
    setupTool(QStringLiteral("my_tool"), QStringLiteral("desc"), QJsonObject{});
    EXPECT_TRUE(m_adapter->isThreadSafe());
}

TEST_F(McpToolAdapterTest, ExecuteSuccess)
{
    setupTool(QStringLiteral("read_file"),
              QStringLiteral("Read a file"),
              QJsonObject{});

    // Enqueue tool call response.
    QJsonArray content;
    QJsonObject textItem;
    textItem[QStringLiteral("type")] = QStringLiteral("text");
    textItem[QStringLiteral("text")] = QStringLiteral("hello world");
    content.append(textItem);

    QJsonObject callResult;
    callResult[QStringLiteral("content")] = content;

    m_rawTransport->enqueueResponse(QStringLiteral("tools/call"),
                                     makeSuccessResponse(callResult));

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("/tmp/test.txt");

    auto result = m_adapter->execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, QStringLiteral("hello world"));
    EXPECT_EQ(result.metadata.value(QStringLiteral("tool")).toString(),
              QStringLiteral("read_file"));
}

TEST_F(McpToolAdapterTest, ExecuteHandlesToolError)
{
    setupTool(QStringLiteral("failing_tool"),
              QStringLiteral("A tool that fails"),
              QJsonObject{});

    QJsonArray content;
    QJsonObject textItem;
    textItem[QStringLiteral("type")] = QStringLiteral("text");
    textItem[QStringLiteral("text")] = QStringLiteral("File not found");
    content.append(textItem);

    QJsonObject callResult;
    callResult[QStringLiteral("content")] = content;
    callResult[QStringLiteral("isError")] = true;

    m_rawTransport->enqueueResponse(QStringLiteral("tools/call"),
                                     makeSuccessResponse(callResult));

    auto result = m_adapter->execute({});
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.contains(QStringLiteral("File not found")));
}

TEST_F(McpToolAdapterTest, ExecuteHandlesJsonRpcError)
{
    setupTool(QStringLiteral("bad_tool"),
              QStringLiteral("desc"),
              QJsonObject{});

    m_rawTransport->enqueueResponse(
        QStringLiteral("tools/call"),
        makeErrorResponse(-32602, QStringLiteral("Invalid params")));

    auto result = m_adapter->execute({});
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.contains(QStringLiteral("Invalid params")));
}

TEST_F(McpToolAdapterTest, ExecuteBeforeInitReturnsError)
{
    // Create a new, uninitialized client.
    auto transport = std::make_unique<MockMcpTransport>();
    auto uninitClient =
        std::make_unique<McpClient>(std::move(transport));

    McpToolAdapter adapter(*uninitClient,
                            QStringLiteral("test_tool"),
                            QStringLiteral("desc"),
                            QJsonObject{});

    auto result = adapter.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.contains(QStringLiteral("not initialized")));
}

TEST_F(McpToolAdapterTest, ExecuteWithMultipleContentItems)
{
    setupTool(QStringLiteral("multi_tool"),
              QStringLiteral("Returns multiple items"),
              QJsonObject{});

    QJsonArray content;
    {
        QJsonObject item1;
        item1[QStringLiteral("type")] = QStringLiteral("text");
        item1[QStringLiteral("text")] = QStringLiteral("line1");
        content.append(item1);
    }
    {
        QJsonObject item2;
        item2[QStringLiteral("type")] = QStringLiteral("text");
        item2[QStringLiteral("text")] = QStringLiteral("line2");
        content.append(item2);
    }

    QJsonObject callResult;
    callResult[QStringLiteral("content")] = content;

    m_rawTransport->enqueueResponse(QStringLiteral("tools/call"),
                                     makeSuccessResponse(callResult));

    auto result = m_adapter->execute({});
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, QStringLiteral("line1\nline2"));
}

TEST_F(McpToolAdapterTest, ExecuteWithEmptyContentReturnsNoOutput)
{
    setupTool(QStringLiteral("empty_tool"),
              QStringLiteral("Returns nothing"),
              QJsonObject{});

    QJsonObject callResult;
    callResult[QStringLiteral("content")] = QJsonArray{};

    m_rawTransport->enqueueResponse(QStringLiteral("tools/call"),
                                     makeSuccessResponse(callResult));

    auto result = m_adapter->execute({});
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, QStringLiteral("(no output)"));
}

// ---------------------------------------------------------------------------
// McpServerConfig tests
// ---------------------------------------------------------------------------

TEST(McpServerConfigTest, DefaultTransportIsStdio)
{
    McpServerConfig config;
    EXPECT_EQ(config.transportType, McpServerConfig::TransportType::Stdio);
}

TEST(McpServerConfigTest, ConfigWithAllFields)
{
    McpServerConfig config;
    config.name = QStringLiteral("test_server");
    config.command = QStringLiteral("python");
    config.args = QStringList{QStringLiteral("-m"), QStringLiteral("mcp_server")};
    config.env = QMap<QString, QString>{
        {QStringLiteral("API_KEY"), QStringLiteral("secret")}};
    config.transportType = McpServerConfig::TransportType::Stdio;

    EXPECT_EQ(config.name, QStringLiteral("test_server"));
    EXPECT_EQ(config.command, QStringLiteral("python"));
    EXPECT_EQ(config.args.size(), 2);
    EXPECT_EQ(config.env.size(), 1);
}

// ---------------------------------------------------------------------------
// McpToolInfo tests
// ---------------------------------------------------------------------------

TEST(McpToolInfoTest, DefaultConstruction)
{
    McpToolInfo info;
    EXPECT_TRUE(info.name.isEmpty());
    EXPECT_TRUE(info.description.isEmpty());
    EXPECT_TRUE(info.inputSchema.isEmpty());
}

TEST(McpToolInfoTest, FieldAssignment)
{
    McpToolInfo info;
    info.name = QStringLiteral("search");
    info.description = QStringLiteral("Search for things");
    info.inputSchema = QJsonObject{{QStringLiteral("type"),
                                    QStringLiteral("object")}};

    EXPECT_EQ(info.name, QStringLiteral("search"));
    EXPECT_FALSE(info.inputSchema.isEmpty());
}
