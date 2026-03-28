#include <gtest/gtest.h>

#include <QJsonArray>

#include "core/error_codes.h"
#include "core/types.h"
#include "harness/tools/diagnostic_tool.h"

using namespace act::core;

class DiagnosticToolTest : public ::testing::Test
{
protected:
    act::harness::DiagnosticTool tool;
};

TEST_F(DiagnosticToolTest, NameAndDescription)
{
    EXPECT_EQ(tool.name(), QStringLiteral("diagnostic"));
    EXPECT_FALSE(tool.description().isEmpty());
}

TEST_F(DiagnosticToolTest, PermissionLevelIsRead)
{
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);
}

TEST_F(DiagnosticToolTest, ParseMsvcError)
{
    QJsonObject params;
    params[QStringLiteral("output")] =
        QStringLiteral("src/main.cpp(42,10): error C2065: "
                       "'x': undeclared identifier");

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    auto diags = result.metadata.value(QStringLiteral("diagnostics"))
                     .toArray();
    ASSERT_EQ(diags.size(), 1);

    auto d = diags[0].toObject();
    EXPECT_EQ(d.value(QStringLiteral("file")).toString(),
              QStringLiteral("src/main.cpp"));
    EXPECT_EQ(d.value(QStringLiteral("line")).toInt(), 42);
    EXPECT_EQ(d.value(QStringLiteral("col")).toInt(), 10);
    EXPECT_EQ(d.value(QStringLiteral("severity")).toString(),
              QStringLiteral("error"));
    EXPECT_EQ(d.value(QStringLiteral("code")).toString(),
              QStringLiteral("C2065"));
    EXPECT_TRUE(d.value(QStringLiteral("message"))
                    .toString()
                    .contains(QStringLiteral("undeclared")));
}

TEST_F(DiagnosticToolTest, ParseMsvcWarning)
{
    QJsonObject params;
    params[QStringLiteral("output")] =
        QStringLiteral("src/util.cpp(15,5): warning C4996: "
                       "'deprecated_func': deprecated");

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    auto diags = result.metadata.value(QStringLiteral("diagnostics"))
                     .toArray();
    ASSERT_EQ(diags.size(), 1);

    auto d = diags[0].toObject();
    EXPECT_EQ(d.value(QStringLiteral("severity")).toString(),
              QStringLiteral("warning"));
    EXPECT_EQ(d.value(QStringLiteral("code")).toString(),
              QStringLiteral("C4996"));
    EXPECT_TRUE(d.value(QStringLiteral("message"))
                    .toString()
                    .contains(QStringLiteral("deprecated")));
}

TEST_F(DiagnosticToolTest, ParseMsvcFatalError)
{
    QJsonObject params;
    params[QStringLiteral("output")] =
        QStringLiteral("src/parser.cpp(100,1): fatal error C1083: "
                       "Cannot open include file: 'missing.h'");

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    auto diags = result.metadata.value(QStringLiteral("diagnostics"))
                     .toArray();
    ASSERT_EQ(diags.size(), 1);

    auto d = diags[0].toObject();
    EXPECT_EQ(d.value(QStringLiteral("severity")).toString(),
              QStringLiteral("fatal error"));
    EXPECT_EQ(d.value(QStringLiteral("code")).toString(),
              QStringLiteral("C1083"));
    EXPECT_EQ(d.value(QStringLiteral("line")).toInt(), 100);
}

TEST_F(DiagnosticToolTest, ParseGccError)
{
    QJsonObject params;
    params[QStringLiteral("output")] =
        QStringLiteral("src/main.cpp:42:10: error: 'x' was not "
                       "declared in this scope");

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    auto diags = result.metadata.value(QStringLiteral("diagnostics"))
                     .toArray();
    ASSERT_EQ(diags.size(), 1);

    auto d = diags[0].toObject();
    EXPECT_EQ(d.value(QStringLiteral("file")).toString(),
              QStringLiteral("src/main.cpp"));
    EXPECT_EQ(d.value(QStringLiteral("line")).toInt(), 42);
    EXPECT_EQ(d.value(QStringLiteral("col")).toInt(), 10);
    EXPECT_EQ(d.value(QStringLiteral("severity")).toString(),
              QStringLiteral("error"));
    EXPECT_TRUE(d.value(QStringLiteral("code")).toString().isEmpty());
    EXPECT_TRUE(d.value(QStringLiteral("message"))
                    .toString()
                    .contains(QStringLiteral("not declared")));
}

TEST_F(DiagnosticToolTest, ParseGccWarning)
{
    QJsonObject params;
    params[QStringLiteral("output")] =
        QStringLiteral("src/util.cpp:15:5: warning: unused variable "
                       "'count' [-Wunused-variable]");

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    auto diags = result.metadata.value(QStringLiteral("diagnostics"))
                     .toArray();
    ASSERT_EQ(diags.size(), 1);

    auto d = diags[0].toObject();
    EXPECT_EQ(d.value(QStringLiteral("severity")).toString(),
              QStringLiteral("warning"));
    EXPECT_TRUE(d.value(QStringLiteral("message"))
                    .toString()
                    .contains(QStringLiteral("unused variable")));
}

TEST_F(DiagnosticToolTest, ParseClangNote)
{
    QJsonObject params;
    params[QStringLiteral("output")] =
        QStringLiteral("src/main.cpp:42:10: note: previous "
                       "declaration is here");

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    auto diags = result.metadata.value(QStringLiteral("diagnostics"))
                     .toArray();
    ASSERT_EQ(diags.size(), 1);

    auto d = diags[0].toObject();
    EXPECT_EQ(d.value(QStringLiteral("severity")).toString(),
              QStringLiteral("note"));
    EXPECT_EQ(d.value(QStringLiteral("line")).toInt(), 42);
}

TEST_F(DiagnosticToolTest, MultipleDiagnostics)
{
    QString output = QStringLiteral(
        "src/a.cpp(10,1): error C2065: 'foo' undeclared\n"
        "src/a.cpp(20,5): warning C4996: deprecated\n"
        "src/b.cpp(5,3): error C2143: syntax error");

    QJsonObject params;
    params[QStringLiteral("output")] = output;

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    auto diags = result.metadata.value(QStringLiteral("diagnostics"))
                     .toArray();
    ASSERT_EQ(diags.size(), 3);

    EXPECT_EQ(diags[0].toObject().value(QStringLiteral("severity")).toString(),
              QStringLiteral("error"));
    EXPECT_EQ(diags[1].toObject().value(QStringLiteral("severity")).toString(),
              QStringLiteral("warning"));
    EXPECT_EQ(diags[2].toObject().value(QStringLiteral("severity")).toString(),
              QStringLiteral("error"));
}

TEST_F(DiagnosticToolTest, MixedFormats)
{
    QString output = QStringLiteral(
        "src/a.cpp(10,1): error C2065: 'foo' undeclared\n"
        "src/b.cpp:20:5: error: 'bar' was not declared\n"
        "src/c.cpp:30:10: note: previous declaration is here");

    QJsonObject params;
    params[QStringLiteral("output")] = output;

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    auto diags = result.metadata.value(QStringLiteral("diagnostics"))
                     .toArray();
    ASSERT_EQ(diags.size(), 3);

    // First: MSVC format
    EXPECT_EQ(diags[0].toObject().value(QStringLiteral("code")).toString(),
              QStringLiteral("C2065"));
    // Second: GCC format (no code)
    EXPECT_TRUE(
        diags[1].toObject().value(QStringLiteral("code")).toString().isEmpty());
    // Third: Clang note
    EXPECT_EQ(diags[2].toObject().value(QStringLiteral("severity")).toString(),
              QStringLiteral("note"));
}

TEST_F(DiagnosticToolTest, NoDiagnostics)
{
    QJsonObject params;
    params[QStringLiteral("output")] =
        QStringLiteral("Build started.\nCompiling 3 files...\nBuild "
                       "finished successfully.");

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    auto diags = result.metadata.value(QStringLiteral("diagnostics"))
                     .toArray();
    EXPECT_EQ(diags.size(), 0);
}

TEST_F(DiagnosticToolTest, MissingOutputParameter)
{
    auto result = tool.execute({});
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, errors::INVALID_PARAMS);
}

TEST_F(DiagnosticToolTest, EmptyOutput)
{
    QJsonObject params;
    params[QStringLiteral("output")] = QString();

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    auto diags = result.metadata.value(QStringLiteral("diagnostics"))
                     .toArray();
    EXPECT_EQ(diags.size(), 0);
    EXPECT_EQ(result.metadata.value(QStringLiteral("errorCount")).toInt(), 0);
    EXPECT_EQ(result.metadata.value(QStringLiteral("warningCount")).toInt(), 0);
    EXPECT_EQ(result.metadata.value(QStringLiteral("noteCount")).toInt(), 0);
}

TEST_F(DiagnosticToolTest, CountsInMetadata)
{
    QString output = QStringLiteral(
        "src/a.cpp(10,1): error C2065: 'foo' undeclared\n"
        "src/a.cpp(20,5): warning C4996: deprecated\n"
        "src/a.cpp(30,3): error C2143: syntax error\n"
        "src/a.cpp:35:1: note: previous declaration is here\n"
        "src/a.cpp:40:1: warning: unused variable");

    QJsonObject params;
    params[QStringLiteral("output")] = output;

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success);

    EXPECT_EQ(result.metadata.value(QStringLiteral("errorCount")).toInt(), 2);
    EXPECT_EQ(result.metadata.value(QStringLiteral("warningCount")).toInt(), 2);
    EXPECT_EQ(result.metadata.value(QStringLiteral("noteCount")).toInt(), 1);
}

TEST_F(DiagnosticToolTest, SchemaRequiresOutput)
{
    QJsonObject schema = tool.schema();
    auto required = schema.value(QStringLiteral("required")).toArray();

    bool hasOutput = false;
    for (const auto &item : required)
    {
        if (item.toString() == QStringLiteral("output"))
            hasOutput = true;
    }
    EXPECT_TRUE(hasOutput);
}
