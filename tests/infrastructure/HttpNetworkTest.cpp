#include <gtest/gtest.h>

#include "infrastructure/http_network.h"

using namespace act::infrastructure;

TEST(HttpNetworkTest, DefaultConstruction)
{
    HttpNetwork net;
    // Should not crash on construction
}

TEST(HttpNetworkTest, SetBaseUrl)
{
    HttpNetwork net;
    net.setBaseUrl(QStringLiteral("https://api.example.com"));
    // No crash expected
}

TEST(HttpNetworkTest, SetProxy)
{
    HttpNetwork net;
    net.setProxy(QStringLiteral("127.0.0.1"), 7890);
    // No crash expected
}

TEST(HttpNetworkTest, SetTimeout)
{
    HttpNetwork net;
    net.setTimeoutSeconds(30);
    // No crash expected
}

TEST(HttpNetworkTest, SetDefaultHeaders)
{
    HttpNetwork net;
    QMap<QString, QString> headers;
    headers[QStringLiteral("X-Custom")] = QStringLiteral("value");
    net.setDefaultHeaders(headers);
    // No crash expected
}

TEST(HttpNetworkTest, HttpRequestInvalidUrl)
{
    HttpNetwork net;
    net.setBaseUrl(QStringLiteral("not-a-valid-url"));

    int statusCode = 0;
    QByteArray responseBody;
    bool result = net.httpRequest(QByteArray(), {}, statusCode, responseBody);

    EXPECT_FALSE(result);
}

TEST(HttpNetworkTest, SseRequestInvalidUrl)
{
    HttpNetwork net;
    net.setBaseUrl(QStringLiteral(""));

    bool called = false;
    bool result = net.sseRequest(
        QByteArray(),
        {},
        [](const SseEvent &) {},
        [&](int, const QByteArray &) { called = true; },
        [](QString code, QString msg) {
            // Empty URL will fail at connection time
            EXPECT_FALSE(code.isEmpty());
        });

    EXPECT_FALSE(result);
}

TEST(HttpNetworkTest, SseRequestNoOnComplete)
{
    HttpNetwork net;
    net.setBaseUrl(QStringLiteral(""));

    // Should not crash even with null onComplete
    bool result = net.sseRequest(
        QByteArray(),
        {},
        [](const SseEvent &) {},
        {},
        [](QString, QString) {});

    EXPECT_FALSE(result);
}

TEST(HttpNetworkTest, CancelDoesNotCrash)
{
    HttpNetwork net;
    net.cancel();
    // No crash expected
}
