#include <gtest/gtest.h>

#include <QStringList>
#include <memory>

#include "core/types.h"
#include "framework/command_registry.h"
#include "framework/commands/doctor_command.h"
#include "infrastructure/interfaces.h"

using namespace act::framework;
using namespace act::framework::commands;

namespace {

/// Mock IProcess that returns preconfigured outputs based on command name.
class MockDoctorProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int /*timeoutMs*/ = 30000) override
    {
        lastCommand = command;
        lastArgs = args;

        if (command == QStringLiteral("cmake"))
            callback(0, QStringLiteral("cmake version 4.2.3\n\nsome extra"));
        else if (command == QStringLiteral("ninja"))
            callback(0, QStringLiteral("1.12.1"));
        else if (command == QStringLiteral("where"))
            callback(0, QStringLiteral("C:\\Program Files\\cl.exe"));
        else if (command == QStringLiteral("git"))
            callback(0, QStringLiteral("git version 2.47.0"));
        else
            callback(1, QString());
    }

    void cancel() override {}

    QString lastCommand;
    QStringList lastArgs;
};

} // anonymous namespace

// ============================================================
// DoctorCommand Tests
// ============================================================

class DoctorCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockProcess = std::make_unique<MockDoctorProcess>();
    }

    void TearDown() override
    {
        mockProcess.reset();
    }

    std::unique_ptr<MockDoctorProcess> mockProcess;
    QStringList capturedOutput;
};

TEST_F(DoctorCommandTest, RegistersWithRegistry)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DoctorCommand::registerTo(registry, *mockProcess, output);

    EXPECT_TRUE(registry.hasCommand(QStringLiteral("doctor")));
    auto info = registry.getCommand(QStringLiteral("doctor"));
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->name, QStringLiteral("doctor"));
    EXPECT_FALSE(info->description.isEmpty());
}

TEST_F(DoctorCommandTest, ExecutesAndOutputsLines)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DoctorCommand::registerTo(registry, *mockProcess, output);

    bool executed = registry.execute(QStringLiteral("doctor"), {});
    EXPECT_TRUE(executed);
    EXPECT_FALSE(capturedOutput.isEmpty());
}

TEST_F(DoctorCommandTest, OutputContainsHeader)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DoctorCommand::registerTo(registry, *mockProcess, output);
    registry.execute(QStringLiteral("doctor"), {});

    bool foundHeader = false;
    for (const auto &line : capturedOutput)
    {
        if (line.contains(QStringLiteral("Environment Diagnostics")))
            foundHeader = true;
    }
    EXPECT_TRUE(foundHeader);
}

TEST_F(DoctorCommandTest, ChecksCMakeVersion)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DoctorCommand::registerTo(registry, *mockProcess, output);
    registry.execute(QStringLiteral("doctor"), {});

    bool foundCMake = false;
    for (const auto &line : capturedOutput)
    {
        if (line.contains(QStringLiteral("CMake")))
            foundCMake = true;
    }
    EXPECT_TRUE(foundCMake);
}

TEST_F(DoctorCommandTest, ChecksAllDiagnostics)
{
    CommandRegistry registry;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DoctorCommand::registerTo(registry, *mockProcess, output);
    registry.execute(QStringLiteral("doctor"), {});

    // Should have checks for CMake, Ninja, Qt, MSVC, vcpkg, Git, API keys, Disk
    QStringList expected = {
        QStringLiteral("CMake"),
        QStringLiteral("Ninja"),
        QStringLiteral("Qt"),
        QStringLiteral("MSVC"),
        QStringLiteral("vcpkg"),
        QStringLiteral("Git"),
        QStringLiteral("API keys"),
        QStringLiteral("Disk space"),
    };

    for (const auto &name : expected)
    {
        bool found = false;
        for (const auto &line : capturedOutput)
        {
            if (line.contains(name))
                found = true;
        }
        EXPECT_TRUE(found) << "Missing diagnostic check: " << name.toStdString();
    }
}

TEST_F(DoctorCommandTest, ReturnsTrueFromHandler)
{
    CommandRegistry registry;
    bool handlerResult = false;
    auto output = [this](const QString &line) { capturedOutput.append(line); };

    DoctorCommand::registerTo(registry, *mockProcess, output);

    auto info = registry.getCommand(QStringLiteral("doctor"));
    ASSERT_NE(info, nullptr);
    handlerResult = info->handler({});
    EXPECT_TRUE(handlerResult);
}

// ============================================================
// DoctorCommand with process failures
// ============================================================

class MockFailingProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString & /*command*/,
                 const QStringList & /*args*/,
                 std::function<void(int, QString)> callback,
                 int /*timeoutMs*/ = 30000) override
    {
        // All commands fail
        callback(1, QString());
    }

    void cancel() override {}
};

TEST(DoctorCommandFailureTest, HandlesAllCommandsFailing)
{
    MockFailingProcess failProcess;
    CommandRegistry registry;
    QStringList output;

    DoctorCommand::registerTo(registry, failProcess,
        [&output](const QString &line) { output.append(line); });

    registry.execute(QStringLiteral("doctor"), {});

    // Should still produce output with FAIL/WARN entries
    EXPECT_FALSE(output.isEmpty());

    bool hasFail = false;
    for (const auto &line : output)
    {
        if (line.contains(QStringLiteral("FAIL")) || line.contains(QStringLiteral("WARN")))
            hasFail = true;
    }
    EXPECT_TRUE(hasFail);
}
