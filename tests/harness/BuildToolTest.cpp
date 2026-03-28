#include <gtest/gtest.h>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/build_tool.h"

using namespace act::core;
using namespace act::core::errors;

// Mock IProcess for testing build tool
class MockBuildProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int /*timeoutMs*/ = 30000) override
    {
        m_lastCommand = command;
        m_lastArgs = args;

        if (command == QStringLiteral("cmd.exe"))
            callback(m_exitCode, m_output);
        else
            callback(1, QStringLiteral("unexpected command"));
    }

    void cancel() override {}

    // Test control
    int m_exitCode = 0;
    QString m_output;
    QString m_lastCommand;
    QStringList m_lastArgs;
};

TEST(BuildToolTest, NameAndMetadata)
{
    MockBuildProcess proc;
    act::harness::BuildTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("build"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Exec);
    EXPECT_FALSE(tool.isThreadSafe());
}

TEST(BuildToolTest, DefaultFullBuild)
{
    MockBuildProcess proc;
    proc.m_output = QStringLiteral("[100%] Built target aictl\n");
    act::harness::BuildTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_EQ(proc.m_lastCommand, QStringLiteral("cmd.exe"));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("/c")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("_build.bat")));
    // No mode flag for full build
    EXPECT_EQ(proc.m_lastArgs.size(), 2);
}

TEST(BuildToolTest, BuildOnlyMode)
{
    MockBuildProcess proc;
    proc.m_output = QStringLiteral("[100%] Built target aictl\n");
    act::harness::BuildTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("mode")] = QStringLiteral("build-only");
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("--build")));
}

TEST(BuildToolTest, NoTestMode)
{
    MockBuildProcess proc;
    proc.m_output = QStringLiteral("[100%] Built target aictl\n");
    act::harness::BuildTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("mode")] = QStringLiteral("no-test");
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("--no-test")));
}

TEST(BuildToolTest, ConfigureOnlyMode)
{
    MockBuildProcess proc;
    proc.m_output = QStringLiteral("-- Configuring done\n");
    act::harness::BuildTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("mode")] = QStringLiteral("configure-only");
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("--configure")));
}

TEST(BuildToolTest, InvalidMode)
{
    MockBuildProcess proc;
    act::harness::BuildTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("mode")] = QStringLiteral("invalid-mode");
    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(BuildToolTest, BuildFailure)
{
    MockBuildProcess proc;
    proc.m_exitCode = 1;
    proc.m_output = QStringLiteral(
        "src/foo.cpp(42,10): error C2065: 'x': undeclared identifier\n"
        "FAILED: build\n");
    act::harness::BuildTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, COMMAND_FAILED);
    EXPECT_TRUE(result.error.contains(QStringLiteral("error")));
    EXPECT_EQ(result.metadata.value(QStringLiteral("errorCount")).toInt(), 1);
}

TEST(BuildToolTest, ParseMultipleErrors)
{
    MockBuildProcess proc;
    proc.m_exitCode = 1;
    proc.m_output = QStringLiteral(
        "src/a.cpp(10,5): error C2039: 'foo' : is not a member of 'Bar'\n"
        "src/b.cpp(20,3): warning C4996: deprecated function\n"
        "src/c.cpp(30,7): error C2143: syntax error\n"
        "FAILED: build\n");
    act::harness::BuildTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.metadata.value(QStringLiteral("errorCount")).toInt(), 2);
    EXPECT_EQ(result.metadata.value(QStringLiteral("warningCount")).toInt(), 1);
}

TEST(BuildToolTest, Timeout)
{
    MockBuildProcess proc;
    proc.m_exitCode = -1;
    act::harness::BuildTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, TIMEOUT);
}
