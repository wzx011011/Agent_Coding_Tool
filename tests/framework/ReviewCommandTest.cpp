#include <gtest/gtest.h>

#include "framework/command_registry.h"
#include "framework/commands/review_command.h"
#include "framework/terminal_style.h"

using namespace act::framework;

namespace {

/// Mock IProcess that records calls and provides scripted diff output.
class MockReviewProcess : public act::infrastructure::IProcess
{
public:
    void execute(const QString &command,
                 const QStringList &args,
                 std::function<void(int, QString)> callback,
                 int timeoutMs = 30000) override
    {
        m_calls.append({command, args});

        if (command == QStringLiteral("git") &&
            args.contains(QStringLiteral("diff")))
        {
            if (m_failDiff)
                callback(1, QStringLiteral("fatal: not a git repository"));
            else
                callback(0, m_diffOutput);
            return;
        }

        callback(1, QStringLiteral("unknown command"));
    }

    void cancel() override {}

    struct Call
    {
        QString command;
        QStringList args;
    };

    [[nodiscard]] const Call *findCall(const QString &cmd,
                                       const QString &subArg = QString()) const
    {
        for (int i = m_calls.size() - 1; i >= 0; --i)
        {
            if (m_calls[i].command == cmd)
            {
                if (subArg.isEmpty() || m_calls[i].args.contains(subArg))
                    return &m_calls[i];
            }
        }
        return nullptr;
    }

    QList<Call> m_calls;
    QString m_diffOutput;
    bool m_failDiff = false;
};

} // anonymous namespace

class ReviewCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_prevColor = TerminalStyle::colorEnabled();
        TerminalStyle::setColorEnabled(false);
    }

    void TearDown() override
    {
        TerminalStyle::setColorEnabled(m_prevColor);
    }

    bool m_prevColor = false;
};

TEST_F(ReviewCommandTest, RegistersCommand)
{
    MockReviewProcess proc;
    CommandRegistry registry;
    QStringList output;

    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    EXPECT_TRUE(registry.hasCommand(QStringLiteral("review")));
    EXPECT_EQ(registry.commandCount(), 1);
}

TEST_F(ReviewCommandTest, NoChanges)
{
    MockReviewProcess proc;
    proc.m_diffOutput = QString(); // Empty diff

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    registry.execute(QStringLiteral("review"), {});

    bool found = false;
    for (const auto &line : output)
    {
        if (line.contains(QStringLiteral("No changes to review")))
            found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(ReviewCommandTest, GitError)
{
    MockReviewProcess proc;
    proc.m_failDiff = true;

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    registry.execute(QStringLiteral("review"), {});

    bool found = false;
    for (const auto &line : output)
    {
        if (line.contains(QStringLiteral("GIT_ERROR")))
            found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(ReviewCommandTest, CleanDiffPasses)
{
    MockReviewProcess proc;
    proc.m_diffOutput =
        QStringLiteral("diff --git a/src/clean.cpp b/src/clean.cpp\n"
                       "index abc..def 100644\n"
                       "--- a/src/clean.cpp\n"
                       "+++ b/src/clean.cpp\n"
                       "@@ -1,3 +1,4 @@\n"
                       " #include <string>\n"
                       "+#include <vector>\n"
                       " \n"
                       " int main() {\n");

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    registry.execute(QStringLiteral("review"), {});

    // Should have a summary section
    QString combined = output.join(QLatin1Char('\n'));
    EXPECT_TRUE(combined.contains(QStringLiteral("Code Review:")));
    EXPECT_TRUE(combined.contains(QStringLiteral("PASS")));
    EXPECT_TRUE(combined.contains(QStringLiteral("Files reviewed: 1")));
}

TEST_F(ReviewCommandTest, DetectsQDebug)
{
    MockReviewProcess proc;
    proc.m_diffOutput =
        QStringLiteral("diff --git a/src/debug.cpp b/src/debug.cpp\n"
                       "index abc..def 100644\n"
                       "--- a/src/debug.cpp\n"
                       "+++ b/src/debug.cpp\n"
                       "@@ -1,3 +1,5 @@\n"
                       " #include <QString>\n"
                       "+    qDebug() << \"debug output\";\n"
                       " \n"
                       " void foo() {}\n");

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    registry.execute(QStringLiteral("review"), {});

    QString combined = output.join(QLatin1Char('\n'));
    EXPECT_TRUE(combined.contains(QStringLiteral("qDebug")));
    EXPECT_TRUE(combined.contains(QStringLiteral("Warning")));
}

TEST_F(ReviewCommandTest, DetectsHardcodedCredential)
{
    MockReviewProcess proc;
    proc.m_diffOutput =
        QStringLiteral("diff --git a/src/config.cpp b/src/config.cpp\n"
                       "index abc..def 100644\n"
                       "--- a/src/config.cpp\n"
                       "+++ b/src/config.cpp\n"
                       "@@ -1,2 +1,3 @@\n"
                       " void setup() {\n"
                       "+    auto password = \"supersecret123\";\n"
                       " }\n");

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    QString combined = output.join(QLatin1Char('\n'));
    EXPECT_TRUE(combined.contains(QStringLiteral("Critical")));
    EXPECT_TRUE(combined.contains(QStringLiteral("credential")));
}

TEST_F(ReviewCommandTest, DetectsRawNew)
{
    MockReviewProcess proc;
    proc.m_diffOutput =
        QStringLiteral("diff --git a/src/main.cpp b/src/main.cpp\n"
                       "index abc..def 100644\n"
                       "--- a/src/main.cpp\n"
                       "+++ b/src/main.cpp\n"
                       "@@ -1,2 +1,3 @@\n"
                       " void setup() {\n"
                       "+    auto p = new Widget();\n"
                       " }\n");

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    QString combined = output.join(QLatin1Char('\n'));
    EXPECT_TRUE(combined.contains(QStringLiteral("raw new")) ||
                combined.contains(QStringLiteral("Raw new")));
}

TEST_F(ReviewCommandTest, DetectsUsingNamespaceStd)
{
    MockReviewProcess proc;
    proc.m_diffOutput =
        QStringLiteral("diff --git a/src/app.cpp b/src/app.cpp\n"
                       "index abc..def 100644\n"
                       "--- a/src/app.cpp\n"
                       "+++ b/src/app.cpp\n"
                       "@@ -1,2 +1,4 @@\n"
                       " #include <string>\n"
                       "+using namespace std;\n"
                       "+\n"
                       " int main() { return 0; }\n");

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    QString combined = output.join(QLatin1Char('\n'));
    EXPECT_TRUE(combined.contains(QStringLiteral("using namespace std")));
}

TEST_F(ReviewCommandTest, StagedFlagPassedToGit)
{
    MockReviewProcess proc;
    proc.m_diffOutput =
        QStringLiteral("diff --git a/src/a.cpp b/src/a.cpp\n"
                       "index abc..def 100644\n"
                       "--- a/src/a.cpp\n"
                       "+++ b/src/a.cpp\n"
                       "@@ -1 +1,2 @@\n"
                       "+int x = 0;\n");

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    registry.execute(QStringLiteral("review"), {QStringLiteral("--staged")});

    // Verify that --staged was passed to git diff
    auto *diffCall = proc.findCall(QStringLiteral("git"), QStringLiteral("diff"));
    ASSERT_NE(diffCall, nullptr);
    EXPECT_TRUE(diffCall->args.contains(QStringLiteral("--staged")));
}

TEST_F(ReviewCommandTest, PathArgumentPassedToGit)
{
    MockReviewProcess proc;
    proc.m_diffOutput =
        QStringLiteral("diff --git a/src/a.cpp b/src/a.cpp\n"
                       "index abc..def 100644\n"
                       "--- a/src/a.cpp\n"
                       "+++ b/src/a.cpp\n"
                       "@@ -1 +1,2 @@\n"
                       "+int x = 0;\n");

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    registry.execute(QStringLiteral("review"), {QStringLiteral("src/a.cpp")});

    // Verify that the path was passed after --
    auto *diffCall = proc.findCall(QStringLiteral("git"), QStringLiteral("diff"));
    ASSERT_NE(diffCall, nullptr);
    EXPECT_TRUE(diffCall->args.contains(QStringLiteral("--")));
    EXPECT_TRUE(diffCall->args.contains(QStringLiteral("src/a.cpp")));
}

TEST_F(ReviewCommandTest, MultipleIssuesReported)
{
    MockReviewProcess proc;
    proc.m_diffOutput =
        QStringLiteral("diff --git a/src/bad.cpp b/src/bad.cpp\n"
                       "index abc..def 100644\n"
                       "--- a/src/bad.cpp\n"
                       "+++ b/src/bad.cpp\n"
                       "@@ -1,2 +1,6 @@\n"
                       "+using namespace std;\n"
                       "+    qDebug() << \"hello\";\n"
                       "+    auto p = new Thing();\n"
                       "+    auto password = \"secret1234\";\n"
                       " \n");

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    registry.execute(QStringLiteral("review"), {});

    QString combined = output.join(QLatin1Char('\n'));
    EXPECT_TRUE(combined.contains(QStringLiteral("Critical")));
    EXPECT_TRUE(combined.contains(QStringLiteral("Warning")));
    EXPECT_TRUE(combined.contains(QStringLiteral("NEEDS ATTENTION")));
    EXPECT_TRUE(combined.contains(QStringLiteral("Summary")));
}

TEST_F(ReviewCommandTest, SummaryFormat)
{
    MockReviewProcess proc;
    proc.m_diffOutput =
        QStringLiteral("diff --git a/src/one.cpp b/src/one.cpp\n"
                       "index abc..def 100644\n"
                       "--- a/src/one.cpp\n"
                       "+++ b/src/one.cpp\n"
                       "@@ -1 +1,2 @@\n"
                       "+int x = 0;\n"
                       "diff --git a/src/two.cpp b/src/two.cpp\n"
                       "index abc..def 100644\n"
                       "--- a/src/two.cpp\n"
                       "+++ b/src/two.cpp\n"
                       "@@ -1 +1,2 @@\n"
                       "+int y = 0;\n");

    CommandRegistry registry;
    QStringList output;
    commands::ReviewCommand::registerTo(
        registry, proc,
        [&](const QString &line) { output.append(line); });

    registry.execute(QStringLiteral("review"), {});

    QString combined = output.join(QLatin1Char('\n'));
    EXPECT_TRUE(combined.contains(QStringLiteral("2 file(s)")));
    EXPECT_TRUE(combined.contains(QStringLiteral("Files reviewed: 2")));
}
