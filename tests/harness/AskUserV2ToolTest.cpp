#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>

#include "core/error_codes.h"
#include "harness/tools/ask_user_v2_tool.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

TEST(AskUserV2ToolTest, NameAndDescription)
{
    AskUserV2Tool tool;
    EXPECT_EQ(tool.name(), QStringLiteral("ask_user_v2"));
    EXPECT_FALSE(tool.description().isEmpty());
    EXPECT_EQ(tool.permissionLevel(), PermissionLevel::Read);
    EXPECT_TRUE(tool.isThreadSafe());
}

TEST(AskUserV2ToolTest, SchemaRequiresQuestions)
{
    AskUserV2Tool tool;
    auto schema = tool.schema();

    auto required = schema.value(QStringLiteral("required")).toArray();
    ASSERT_EQ(required.size(), 1);
    EXPECT_EQ(required.at(0).toString(), QStringLiteral("questions"));

    auto props = schema.value(QStringLiteral("properties")).toObject();
    EXPECT_TRUE(props.contains(QStringLiteral("questions")));
}

TEST(AskUserV2ToolTest, SchemaQuestionsArrayBounds)
{
    AskUserV2Tool tool;
    auto schema = tool.schema();

    auto props = schema.value(QStringLiteral("properties")).toObject();
    auto questionsSchema =
        props.value(QStringLiteral("questions")).toObject();
    EXPECT_EQ(questionsSchema.value(QStringLiteral("minItems")).toInt(), 1);
    EXPECT_EQ(questionsSchema.value(QStringLiteral("maxItems")).toInt(), 4);
}

TEST(AskUserV2ToolTest, ExecuteWithSingleQuestion)
{
    AskUserV2Tool tool;

    QJsonArray options;
    {
        QJsonObject opt1;
        opt1[QStringLiteral("label")] = QStringLiteral("Yes");
        opt1[QStringLiteral("description")] = QStringLiteral("Proceed");
        options.append(opt1);

        QJsonObject opt2;
        opt2[QStringLiteral("label")] = QStringLiteral("No");
        options.append(opt2);
    }

    QJsonObject question;
    question[QStringLiteral("question")] =
        QStringLiteral("Continue?");
    question[QStringLiteral("options")] = options;

    QJsonObject params;
    params[QStringLiteral("questions")] = QJsonArray{question};

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_EQ(result.output, QStringLiteral("__WAITING_USER_INPUT__"));

    auto answers =
        result.metadata.value(QStringLiteral("answers")).toObject();
    EXPECT_TRUE(answers.contains(QStringLiteral("Q1")));

    auto q1 = answers.value(QStringLiteral("Q1")).toObject();
    EXPECT_EQ(q1.value(QStringLiteral("question")).toString(),
              QStringLiteral("Continue?"));
    EXPECT_EQ(
        q1.value(QStringLiteral("options")).toArray().size(),
        2);
}

TEST(AskUserV2ToolTest, ExecuteWithMultipleQuestions)
{
    AskUserV2Tool tool;

    auto makeOptions = [](const QStringList &labels) {
        QJsonArray arr;
        for (const auto &label : labels)
        {
            QJsonObject opt;
            opt[QStringLiteral("label")] = label;
            arr.append(opt);
        }
        return arr;
    };

    QJsonObject q1;
    q1[QStringLiteral("question")] = QStringLiteral("Q1?");
    q1[QStringLiteral("options")] =
        makeOptions({QStringLiteral("A"), QStringLiteral("B")});

    QJsonObject q2;
    q2[QStringLiteral("question")] = QStringLiteral("Q2?");
    q2[QStringLiteral("options")] =
        makeOptions({QStringLiteral("X"), QStringLiteral("Y")});

    QJsonObject params;
    params[QStringLiteral("questions")] = QJsonArray{q1, q2};

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    auto answers =
        result.metadata.value(QStringLiteral("answers")).toObject();
    EXPECT_TRUE(answers.contains(QStringLiteral("Q1")));
    EXPECT_TRUE(answers.contains(QStringLiteral("Q2")));
}

TEST(AskUserV2ToolTest, MissingQuestionsParameter)
{
    AskUserV2Tool tool;
    QJsonObject params;

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(AskUserV2ToolTest, EmptyQuestionsArray)
{
    AskUserV2Tool tool;
    QJsonObject params;
    params[QStringLiteral("questions")] = QJsonArray{};

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(AskUserV2ToolTest, TooManyQuestions)
{
    AskUserV2Tool tool;

    QJsonObject q;
    q[QStringLiteral("question")] = QStringLiteral("Q?");
    QJsonArray opts;
    opts.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("A")}});
    opts.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("B")}});
    q[QStringLiteral("options")] = opts;

    QJsonArray questions;
    for (int i = 0; i < 5; ++i)
        questions.append(q);

    QJsonObject params;
    params[QStringLiteral("questions")] = questions;

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(AskUserV2ToolTest, EmptyQuestionString)
{
    AskUserV2Tool tool;

    QJsonArray opts;
    opts.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("A")}});
    opts.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("B")}});

    QJsonObject question;
    question[QStringLiteral("question")] = QString();
    question[QStringLiteral("options")] = opts;

    QJsonObject params;
    params[QStringLiteral("questions")] = QJsonArray{question};

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(AskUserV2ToolTest, TooFewOptions)
{
    AskUserV2Tool tool;

    QJsonArray opts;
    opts.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("A")}});

    QJsonObject question;
    question[QStringLiteral("question")] = QStringLiteral("Q?");
    question[QStringLiteral("options")] = opts;

    QJsonObject params;
    params[QStringLiteral("questions")] = QJsonArray{question};

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(AskUserV2ToolTest, TooManyOptions)
{
    AskUserV2Tool tool;

    QJsonArray opts;
    for (int i = 0; i < 5; ++i)
        opts.append(QJsonObject{
            {QStringLiteral("label"), QStringLiteral("Opt%1").arg(i)}});

    QJsonObject question;
    question[QStringLiteral("question")] = QStringLiteral("Q?");
    question[QStringLiteral("options")] = opts;

    QJsonObject params;
    params[QStringLiteral("questions")] = QJsonArray{question};

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(AskUserV2ToolTest, OptionMissingLabel)
{
    AskUserV2Tool tool;

    QJsonArray opts;
    opts.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("A")}});
    // Second option has no label
    opts.append(QJsonObject{
        {QStringLiteral("description"),
         QStringLiteral("No label")}});

    QJsonObject question;
    question[QStringLiteral("question")] = QStringLiteral("Q?");
    question[QStringLiteral("options")] = opts;

    QJsonObject params;
    params[QStringLiteral("questions")] = QJsonArray{question};

    auto result = tool.execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST(AskUserV2ToolTest, HeaderAndPreviewFields)
{
    AskUserV2Tool tool;

    QJsonArray opts;
    opts.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("A")}});
    opts.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("B")}});

    QJsonObject question;
    question[QStringLiteral("question")] = QStringLiteral("Q?");
    question[QStringLiteral("header")] = QStringLiteral("Choice");
    question[QStringLiteral("preview")] = QStringLiteral("code here");
    question[QStringLiteral("options")] = opts;

    QJsonObject params;
    params[QStringLiteral("questions")] = QJsonArray{question};

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    auto answers =
        result.metadata.value(QStringLiteral("answers")).toObject();
    auto q1 = answers.value(QStringLiteral("Q1")).toObject();
    EXPECT_EQ(q1.value(QStringLiteral("header")).toString(),
              QStringLiteral("Choice"));
    EXPECT_EQ(q1.value(QStringLiteral("preview")).toString(),
              QStringLiteral("code here"));
}

TEST(AskUserV2ToolTest, MultiSelectField)
{
    AskUserV2Tool tool;

    QJsonArray opts;
    opts.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("A")}});
    opts.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("B")}});

    QJsonObject question;
    question[QStringLiteral("question")] = QStringLiteral("Select?");
    question[QStringLiteral("multiSelect")] = true;
    question[QStringLiteral("options")] = opts;

    QJsonObject params;
    params[QStringLiteral("questions")] = QJsonArray{question};

    auto result = tool.execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    auto answers =
        result.metadata.value(QStringLiteral("answers")).toObject();
    auto q1 = answers.value(QStringLiteral("Q1")).toObject();
    EXPECT_TRUE(q1.value(QStringLiteral("multiSelect")).toBool());
}
