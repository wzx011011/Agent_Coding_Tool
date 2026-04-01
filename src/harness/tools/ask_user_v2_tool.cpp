#include "harness/tools/ask_user_v2_tool.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness {

QString AskUserV2Tool::name() const
{
    return QStringLiteral("ask_user_v2");
}

QString AskUserV2Tool::description() const
{
    return QStringLiteral(
        "Enhanced user question tool with multi-question, options, "
        "and preview support. Validates and structures questions for "
        "the LLM. The actual user interaction is handled by the "
        "interactive permission flow.");
}

QJsonObject AskUserV2Tool::schema() const
{
    // Options item schema
    QJsonObject optionItem;
    {
        QJsonObject optProps;
        optProps[QStringLiteral("label")] = [] {
            QJsonObject obj;
            obj[QStringLiteral("type")] = QStringLiteral("string");
            obj[QStringLiteral("description")] =
                QStringLiteral("Option label");
            return obj;
        }();
        optProps[QStringLiteral("description")] = [] {
            QJsonObject obj;
            obj[QStringLiteral("type")] = QStringLiteral("string");
            obj[QStringLiteral("description")] =
                QStringLiteral("Why this option");
            return obj;
        }();
        optionItem[QStringLiteral("type")] = QStringLiteral("object");
        optionItem[QStringLiteral("required")] =
            QJsonArray{QStringLiteral("label")};
        optionItem[QStringLiteral("properties")] = optProps;
    }

    // Options array schema
    QJsonObject optionsArray;
    optionsArray[QStringLiteral("type")] = QStringLiteral("array");
    optionsArray[QStringLiteral("description")] =
        QStringLiteral("2-4 options for the user to select from");
    optionsArray[QStringLiteral("minItems")] = 2;
    optionsArray[QStringLiteral("maxItems")] = 4;
    optionsArray[QStringLiteral("items")] = optionItem;

    // Question item schema
    QJsonObject questionItem;
    {
        QJsonObject qProps;
        qProps[QStringLiteral("question")] = [] {
            QJsonObject obj;
            obj[QStringLiteral("type")] = QStringLiteral("string");
            obj[QStringLiteral("description")] =
                QStringLiteral("The question to ask");
            return obj;
        }();
        qProps[QStringLiteral("header")] = [] {
            QJsonObject obj;
            obj[QStringLiteral("type")] = QStringLiteral("string");
            obj[QStringLiteral("description")] =
                QStringLiteral("Short label (max 12 chars)");
            return obj;
        }();
        qProps[QStringLiteral("options")] = optionsArray;
        qProps[QStringLiteral("multiSelect")] = [] {
            QJsonObject obj;
            obj[QStringLiteral("type")] = QStringLiteral("boolean");
            obj[QStringLiteral("description")] =
                QStringLiteral("Allow multiple selections (default: false)");
            obj[QStringLiteral("default")] = false;
            return obj;
        }();
        qProps[QStringLiteral("preview")] = [] {
            QJsonObject obj;
            obj[QStringLiteral("type")] = QStringLiteral("string");
            obj[QStringLiteral("description")] =
                QStringLiteral(
                    "Optional preview content (code snippets, ASCII diagrams)");
            return obj;
        }();
        questionItem[QStringLiteral("type")] = QStringLiteral("object");
        questionItem[QStringLiteral("required")] =
            QJsonArray{QStringLiteral("question"), QStringLiteral("options")};
        questionItem[QStringLiteral("properties")] = qProps;
    }

    // Questions array schema
    QJsonObject questionsArray;
    questionsArray[QStringLiteral("type")] = QStringLiteral("array");
    questionsArray[QStringLiteral("minItems")] = 1;
    questionsArray[QStringLiteral("maxItems")] = 4;
    questionsArray[QStringLiteral("items")] = questionItem;

    // Root schema
    QJsonObject props;
    props[QStringLiteral("questions")] = questionsArray;

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("questions")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult AskUserV2Tool::execute(const QJsonObject &params)
{
    // Validate questions array
    auto questionsVal = params.value(QStringLiteral("questions"));
    if (!questionsVal.isArray())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'questions' must be an array"));
    }

    QJsonArray questions = questionsVal.toArray();
    if (questions.size() < 1 || questions.size() > 4)
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'questions' must contain 1-4 items, got %1")
                .arg(questions.size()));
    }

    QJsonObject answersObj;
    QJsonArray validationErrors;

    for (int i = 0; i < questions.size(); ++i)
    {
        QString qKey = QStringLiteral("Q%1").arg(i + 1);
        auto qObj = questions.at(i).toObject();

        // Validate question string
        auto question = qObj.value(QStringLiteral("question")).toString();
        if (question.isEmpty())
        {
            validationErrors.append(
                QStringLiteral("%1: 'question' must be a non-empty string")
                    .arg(qKey));
            continue;
        }

        // Validate options array
        auto optionsVal = qObj.value(QStringLiteral("options"));
        if (!optionsVal.isArray())
        {
            validationErrors.append(
                QStringLiteral("%1: 'options' must be an array").arg(qKey));
            continue;
        }

        QJsonArray options = optionsVal.toArray();
        if (options.size() < 2 || options.size() > 4)
        {
            validationErrors.append(
                QStringLiteral(
                    "%1: 'options' must contain 2-4 items, got %2")
                    .arg(qKey)
                    .arg(options.size()));
            continue;
        }

        // Validate each option has a label
        bool optionsValid = true;
        QJsonArray validatedOptions;
        for (int j = 0; j < options.size(); ++j)
        {
            auto optObj = options.at(j).toObject();
            auto label = optObj.value(QStringLiteral("label")).toString();
            if (label.isEmpty())
            {
                validationErrors.append(
                    QStringLiteral(
                        "%1: option[%2] must have a non-empty 'label'")
                        .arg(qKey)
                        .arg(j));
                optionsValid = false;
                continue;
            }
            validatedOptions.append(optObj);
        }

        if (!optionsValid)
            continue;

        // Build answer entry
        QJsonObject answerObj;
        answerObj[QStringLiteral("question")] = question;
        answerObj[QStringLiteral("selectedOptions")] = QJsonArray{};
        answerObj[QStringLiteral("options")] = validatedOptions;

        auto header = qObj.value(QStringLiteral("header")).toString();
        if (!header.isEmpty())
            answerObj[QStringLiteral("header")] = header;

        auto multiSelect =
            qObj.value(QStringLiteral("multiSelect")).toBool(false);
        answerObj[QStringLiteral("multiSelect")] = multiSelect;

        auto preview = qObj.value(QStringLiteral("preview")).toString();
        if (!preview.isEmpty())
            answerObj[QStringLiteral("preview")] = preview;

        answersObj[qKey] = answerObj;
    }

    if (!validationErrors.isEmpty())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Validation errors: %1")
                .arg(QJsonDocument(validationErrors).toJson(
                    QJsonDocument::Compact)));
    }

    // Return structured response with the formatted questions
    QJsonObject response;
    response[QStringLiteral("answers")] = answersObj;
    response[QStringLiteral("pause_agent")] = true;
    response[QStringLiteral("type")] = QStringLiteral("ask_user_v2");

    return act::core::ToolResult::ok(
        QStringLiteral("__WAITING_USER_INPUT__"),
        response);
}

act::core::PermissionLevel AskUserV2Tool::permissionLevel() const
{
    return act::core::PermissionLevel::Read;
}

bool AskUserV2Tool::isThreadSafe() const
{
    return true;
}

} // namespace act::harness
