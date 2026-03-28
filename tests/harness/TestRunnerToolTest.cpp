#include <gtest/gtest.h>

#include <QJsonArray>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/test_runner_tool.h"

using namespace act::core;
using namespace act::core::errors;

// Mock IProcess for testing test runner tool
class MockTestProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int /*timeoutMs*/ = 30000) override
    {
        m_lastCommand = command;
        m_lastArgs = args;

        if (command == QStringLiteral("ctest"))
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

TEST(TestRunnerToolTest, NameAndMetadata)
{
    MockTestProcess proc;
    act::harness::TestRunnerTool tool(proc, QStringLiteral("/workspace"));
    EXPECT_EQ(tool.name(), QStringLiteral("test_runner"));
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Exec);
    EXPECT_FALSE(tool.isThreadSafe());
}

TEST(TestRunnerToolTest, RunsAllTests)
{
    MockTestProcess proc;
    proc.m_output = QStringLiteral(
        "    Start 1: FileReadToolTest.NameAndMetadata\n"
        "1/3 Test #1: FileReadToolTest.NameAndMetadata ... Passed\n"
        "    Start 2: FileWriteToolTest.WriteNewFile\n"
        "2/3 Test #2: FileWriteToolTest.WriteNewFile ... Passed\n"
        "    Start 3: FileEditToolTest.EditFile\n"
        "3/3 Test #3: FileEditToolTest.EditFile ... Passed\n"
        "\n100% tests passed, 0 tests failed out of 3\n");
    act::harness::TestRunnerTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_EQ(proc.m_lastCommand, QStringLiteral("ctest"));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("--test-dir")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("build")));
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("--output-on-failure")));
    EXPECT_EQ(result.metadata.value(QStringLiteral("passed")).toInt(), 3);
    EXPECT_EQ(result.metadata.value(QStringLiteral("failed")).toInt(), 0);
    EXPECT_EQ(result.metadata.value(QStringLiteral("total")).toInt(), 3);
}

TEST(TestRunnerToolTest, FilterTests)
{
    MockTestProcess proc;
    proc.m_output = QStringLiteral(
        "    Start 1: FileReadToolTest.NameAndMetadata\n"
        "1/1 Test #1: FileReadToolTest.NameAndMetadata ... Passed\n"
        "\n100% tests passed, 0 tests failed out of 1\n");
    act::harness::TestRunnerTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("filter")] = QStringLiteral("FileRead");
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("-R")));
    int filterIdx = proc.m_lastArgs.indexOf(QStringLiteral("-R"));
    EXPECT_EQ(proc.m_lastArgs.at(filterIdx + 1), QStringLiteral("FileRead"));
}

TEST(TestRunnerToolTest, ExtraArgs)
{
    MockTestProcess proc;
    proc.m_output = QStringLiteral("tests passed\n");
    act::harness::TestRunnerTool tool(proc, QStringLiteral("/workspace"));

    QJsonObject params;
    params[QStringLiteral("extra_args")] = QJsonArray{QStringLiteral("-V")};
    auto result = tool.execute(params);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(proc.m_lastArgs.contains(QStringLiteral("-V")));
}

TEST(TestRunnerToolTest, TestFailure)
{
    MockTestProcess proc;
    proc.m_exitCode = 1;
    proc.m_output = QStringLiteral(
        "    Start 1: GoodTest ... Passed\n"
        "1/2 Test #1: GoodTest ... Passed\n"
        "    Start 2: BadTest ... ***Failed\n"
        "2/2 Test #2: BadTest ... ***Failed\n"
        "\n50% tests passed, 1 tests failed out of 2\n");
    act::harness::TestRunnerTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, COMMAND_FAILED);
    EXPECT_EQ(result.metadata.value(QStringLiteral("passed")).toInt(), 1);
    EXPECT_EQ(result.metadata.value(QStringLiteral("failed")).toInt(), 1);
    auto failedTests = result.metadata.value(QStringLiteral("failedTests")).toArray();
    EXPECT_EQ(failedTests.size(), 1);
}

TEST(TestRunnerToolTest, Timeout)
{
    MockTestProcess proc;
    proc.m_exitCode = -1;
    act::harness::TestRunnerTool tool(proc, QStringLiteral("/workspace"));

    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, TIMEOUT);
}
