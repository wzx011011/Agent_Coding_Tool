#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include "core/error_codes.h"
#include "harness/tools/notebook_edit_tool.h"
#include "mock_file_system.h"

using namespace act::core;
using namespace act::core::errors;
using namespace act::harness;

namespace
{

// Helper: create a minimal .ipynb JSON string
QString makeNotebookJson(const QJsonArray &cells)
{
    QJsonObject nb;
    nb[QStringLiteral("nbformat")] = 4;
    nb[QStringLiteral("nbformat_minor")] = 5;
    nb[QStringLiteral("metadata")] = QJsonObject();
    nb[QStringLiteral("cells")] = cells;
    return QString::fromUtf8(QJsonDocument(nb).toJson(QJsonDocument::Indented));
}

// Helper: create a cell object
QJsonObject makeCell(const QString &type, const QString &id,
                     const QStringList &sourceLines)
{
    QJsonObject cell;
    cell[QStringLiteral("cell_type")] = type;
    cell[QStringLiteral("id")] = id;
    cell[QStringLiteral("metadata")] = QJsonObject();

    QJsonArray sourceArr;
    for (const auto &line : sourceLines)
        sourceArr.append(line);
    cell[QStringLiteral("source")] = sourceArr;

    if (type == QStringLiteral("code"))
    {
        cell[QStringLiteral("execution_count")] = QJsonValue::Null;
        cell[QStringLiteral("outputs")] = QJsonArray();
    }

    return cell;
}

// Helper: write a .ipynb file to the temp workspace
void writeNotebook(const QString &dir, const QString &relativePath,
                   const QJsonArray &cells)
{
    const QString fullPath =
        QDir::cleanPath(dir + QLatin1Char('/') + relativePath);
    QDir().mkpath(QFileInfo(fullPath).absolutePath());

    QFile file(fullPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(makeNotebookJson(cells).toUtf8());
    file.close();
}

// Helper: load cells from a .ipynb file
QJsonArray loadCells(const QString &dir, const QString &relativePath)
{
    const QString fullPath =
        QDir::cleanPath(dir + QLatin1Char('/') + relativePath);
    QFile file(fullPath);
    EXPECT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    EXPECT_EQ(err.error, QJsonParseError::NoError);
    return doc.object().value(QStringLiteral("cells")).toArray();
}

} // anonymous namespace

class NotebookEditToolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tmpDir.emplace();
        ASSERT_TRUE(tmpDir->isValid());
        root = QDir::cleanPath(tmpDir->path());
        fs = std::make_unique<MockFileSystem>(root);
        tool = std::make_unique<NotebookEditTool>(*fs, root);
    }

    void TearDown() override
    {
        tool.reset();
        fs.reset();
        tmpDir.reset();
    }

    // Create a standard test notebook with 2 cells
    void createTestNotebook()
    {
        QJsonArray cells;
        cells.append(makeCell(
            QStringLiteral("code"), QStringLiteral("abc123"),
            {QStringLiteral("print('hello')\n")}));
        cells.append(makeCell(
            QStringLiteral("markdown"), QStringLiteral("def456"),
            {QStringLiteral("# Title\n"), QStringLiteral("Description text\n")}));

        writeNotebook(root, QStringLiteral("test.ipynb"), cells);
    }

    std::optional<QTemporaryDir> tmpDir;
    std::unique_ptr<MockFileSystem> fs;
    std::unique_ptr<NotebookEditTool> tool;
    QString root;
};

// ============================================================
// Metadata tests
// ============================================================

TEST_F(NotebookEditToolTest, NameAndDescription)
{
    EXPECT_EQ(tool->name(), QStringLiteral("notebook_edit"));
    EXPECT_FALSE(tool->description().isEmpty());
}

TEST_F(NotebookEditToolTest, PermissionLevelIsWrite)
{
    EXPECT_EQ(tool->permissionLevel(), PermissionLevel::Write);
}

TEST_F(NotebookEditToolTest, IsNotThreadSafe)
{
    EXPECT_FALSE(tool->isThreadSafe());
}

TEST_F(NotebookEditToolTest, SchemaRequiresPathAndAction)
{
    const auto schema = tool->schema();
    const auto required =
        schema.value(QStringLiteral("required")).toArray();
    EXPECT_EQ(required.size(), 2);
    EXPECT_EQ(required.at(0).toString(), QStringLiteral("path"));
    EXPECT_EQ(required.at(1).toString(), QStringLiteral("action"));

    const auto props =
        schema.value(QStringLiteral("properties")).toObject();
    EXPECT_TRUE(props.contains(QStringLiteral("path")));
    EXPECT_TRUE(props.contains(QStringLiteral("action")));
    EXPECT_TRUE(props.contains(QStringLiteral("cell_id")));
    EXPECT_TRUE(props.contains(QStringLiteral("cell_number")));
    EXPECT_TRUE(props.contains(QStringLiteral("cell_type")));
    EXPECT_TRUE(props.contains(QStringLiteral("new_source")));
    EXPECT_TRUE(props.contains(QStringLiteral("edit_mode")));
    EXPECT_TRUE(props.contains(QStringLiteral("index")));
}

// ============================================================
// Validation tests
// ============================================================

TEST_F(NotebookEditToolTest, MissingPathParameter)
{
    QJsonObject params;
    params[QStringLiteral("action")] = QStringLiteral("list_cells");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(NotebookEditToolTest, MissingActionParameter)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(NotebookEditToolTest, InvalidAction)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("invalid_action");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(NotebookEditToolTest, PathMustEndWithIpynb)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.txt");
    params[QStringLiteral("action")] = QStringLiteral("list_cells");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(NotebookEditToolTest, PathOutsideWorkspace)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("/tmp/outside.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("list_cells");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, OUTSIDE_WORKSPACE);
}

TEST_F(NotebookEditToolTest, PathTraversalOutsideWorkspace)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("../outside.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("list_cells");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, OUTSIDE_WORKSPACE);
}

// ============================================================
// list_cells tests
// ============================================================

TEST_F(NotebookEditToolTest, ListCells)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("list_cells");

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(result.output.contains(QStringLiteral("code")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("markdown")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("abc123")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("def456")));
    EXPECT_EQ(result.metadata.value(QStringLiteral("cell_count")).toInt(), 2);
}

TEST_F(NotebookEditToolTest, ListCellsFileNotFound)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("missing.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("list_cells");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, FILE_NOT_FOUND);
}

// ============================================================
// read_cell tests
// ============================================================

TEST_F(NotebookEditToolTest, ReadCellByNumber)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("read_cell");
    params[QStringLiteral("cell_number")] = 0;

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_EQ(result.output, QStringLiteral("print('hello')\n"));
    EXPECT_EQ(result.metadata.value(QStringLiteral("cell_type")).toString(),
              QStringLiteral("code"));
    EXPECT_EQ(result.metadata.value(QStringLiteral("cell_id")).toString(),
              QStringLiteral("abc123"));
}

TEST_F(NotebookEditToolTest, ReadCellById)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("read_cell");
    params[QStringLiteral("cell_id")] = QStringLiteral("def456");

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(result.output.contains(QStringLiteral("# Title")));
    EXPECT_TRUE(result.output.contains(QStringLiteral("Description text")));
    EXPECT_EQ(result.metadata.value(QStringLiteral("cell_type")).toString(),
              QStringLiteral("markdown"));
}

TEST_F(NotebookEditToolTest, ReadCellNotFound)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("read_cell");
    params[QStringLiteral("cell_id")] = QStringLiteral("nonexistent");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, STRING_NOT_FOUND);
}

TEST_F(NotebookEditToolTest, ReadCellInvalidNumber)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("read_cell");
    params[QStringLiteral("cell_number")] = 99;

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, STRING_NOT_FOUND);
}

// ============================================================
// add_cell tests
// ============================================================

TEST_F(NotebookEditToolTest, AddCellAppend)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("add_cell");
    params[QStringLiteral("new_source")] = QStringLiteral("x = 42");

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(result.output.contains(QStringLiteral("Added")));

    // Verify the cell was added
    const QJsonArray cells =
        loadCells(root, QStringLiteral("test.ipynb"));
    ASSERT_EQ(cells.size(), 3);
    EXPECT_EQ(cells.at(2).toObject()
                  .value(QStringLiteral("cell_type"))
                  .toString(),
              QStringLiteral("code"));
}

TEST_F(NotebookEditToolTest, AddCellAtIndex)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("add_cell");
    params[QStringLiteral("new_source")] = QStringLiteral("# Header");
    params[QStringLiteral("cell_type")] = QStringLiteral("markdown");
    params[QStringLiteral("index")] = 0;

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    const QJsonArray cells =
        loadCells(root, QStringLiteral("test.ipynb"));
    ASSERT_EQ(cells.size(), 3);
    EXPECT_EQ(cells.at(0).toObject()
                  .value(QStringLiteral("cell_type"))
                  .toString(),
              QStringLiteral("markdown"));
}

TEST_F(NotebookEditToolTest, AddCellCreatesNewNotebook)
{
    // No test notebook exists yet
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("new.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("add_cell");
    params[QStringLiteral("new_source")] = QStringLiteral("print('new')");

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(result.metadata.value(QStringLiteral("new_file")).toBool());

    const QJsonArray cells =
        loadCells(root, QStringLiteral("new.ipynb"));
    ASSERT_EQ(cells.size(), 1);
}

TEST_F(NotebookEditToolTest, AddCellMissingSource)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("add_cell");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

// ============================================================
// delete_cell tests
// ============================================================

TEST_F(NotebookEditToolTest, DeleteCellByNumber)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("delete_cell");
    params[QStringLiteral("cell_number")] = 0;

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    const QJsonArray cells =
        loadCells(root, QStringLiteral("test.ipynb"));
    ASSERT_EQ(cells.size(), 1);
    EXPECT_EQ(cells.at(0).toObject()
                  .value(QStringLiteral("id"))
                  .toString(),
              QStringLiteral("def456"));
}

TEST_F(NotebookEditToolTest, DeleteCellById)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("delete_cell");
    params[QStringLiteral("cell_id")] = QStringLiteral("abc123");

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    const QJsonArray cells =
        loadCells(root, QStringLiteral("test.ipynb"));
    ASSERT_EQ(cells.size(), 1);
    EXPECT_EQ(cells.at(0).toObject()
                  .value(QStringLiteral("id"))
                  .toString(),
              QStringLiteral("def456"));
}

TEST_F(NotebookEditToolTest, DeleteCellNotFound)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("delete_cell");
    params[QStringLiteral("cell_id")] = QStringLiteral("nonexistent");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, STRING_NOT_FOUND);
}

TEST_F(NotebookEditToolTest, DeleteCellFileNotFound)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("missing.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("delete_cell");
    params[QStringLiteral("cell_number")] = 0;

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, FILE_NOT_FOUND);
}

// ============================================================
// edit_cell tests
// ============================================================

TEST_F(NotebookEditToolTest, EditCellReplace)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("edit_cell");
    params[QStringLiteral("cell_number")] = 0;
    params[QStringLiteral("new_source")] = QStringLiteral("print('replaced')");

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    // Read it back to verify
    QJsonObject readParams;
    readParams[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    readParams[QStringLiteral("action")] = QStringLiteral("read_cell");
    readParams[QStringLiteral("cell_number")] = 0;

    const auto readResult = tool->execute(readParams);
    ASSERT_TRUE(readResult.success) << readResult.error.toStdString();
    EXPECT_EQ(readResult.output, QStringLiteral("print('replaced')"));
}

TEST_F(NotebookEditToolTest, EditCellInsert)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("edit_cell");
    params[QStringLiteral("cell_number")] = 0;
    params[QStringLiteral("edit_mode")] = QStringLiteral("insert");
    params[QStringLiteral("new_source")] = QStringLiteral("import os");
    params[QStringLiteral("cell_type")] = QStringLiteral("code");

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    const QJsonArray cells =
        loadCells(root, QStringLiteral("test.ipynb"));
    ASSERT_EQ(cells.size(), 3);
    // Original cell at index 0 should still be abc123
    EXPECT_EQ(cells.at(0).toObject()
                  .value(QStringLiteral("id"))
                  .toString(),
              QStringLiteral("abc123"));
}

TEST_F(NotebookEditToolTest, EditCellMissingSource)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("edit_cell");
    params[QStringLiteral("cell_number")] = 0;

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(NotebookEditToolTest, EditCellNotFound)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("edit_cell");
    params[QStringLiteral("cell_number")] = 99;
    params[QStringLiteral("new_source")] = QStringLiteral("test");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, STRING_NOT_FOUND);
}

// ============================================================
// Invalid notebook format tests
// ============================================================

TEST_F(NotebookEditToolTest, InvalidJsonContent)
{
    // Write a non-JSON file with .ipynb extension
    const QString fullPath =
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("bad.ipynb"));
    QFile file(fullPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("this is not json");
    file.close();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("bad.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("list_cells");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(NotebookEditToolTest, WrongNbformat)
{
    // Write a notebook with nbformat 3
    QJsonObject nb;
    nb[QStringLiteral("nbformat")] = 3;
    nb[QStringLiteral("cells")] = QJsonArray();

    const QString fullPath =
        QDir::cleanPath(root + QLatin1Char('/') + QStringLiteral("old.ipynb"));
    QFile file(fullPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(QJsonDocument(nb).toJson());
    file.close();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("old.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("list_cells");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

// ============================================================
// Edge cases
// ============================================================

TEST_F(NotebookEditToolTest, EmptyNotebookListCells)
{
    QJsonArray emptyCells;
    writeNotebook(root, QStringLiteral("empty.ipynb"), emptyCells);

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("empty.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("list_cells");

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(result.output.contains(QStringLiteral("no cells")));
    EXPECT_EQ(result.metadata.value(QStringLiteral("cell_count")).toInt(), 0);
}

TEST_F(NotebookEditToolTest, AddCellMarkdownType)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("add_cell");
    params[QStringLiteral("new_source")] = QStringLiteral("# New Section");
    params[QStringLiteral("cell_type")] = QStringLiteral("markdown");

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    const QJsonArray cells =
        loadCells(root, QStringLiteral("test.ipynb"));
    ASSERT_EQ(cells.size(), 3);
    const auto newCell = cells.at(2).toObject();
    EXPECT_EQ(newCell.value(QStringLiteral("cell_type")).toString(),
              QStringLiteral("markdown"));
    // Markdown cells should NOT have execution_count
    EXPECT_FALSE(newCell.contains(QStringLiteral("execution_count")));
}

TEST_F(NotebookEditToolTest, AddCellRawType)
{
    createTestNotebook();

    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = QStringLiteral("add_cell");
    params[QStringLiteral("new_source")] = QStringLiteral("raw content");
    params[QStringLiteral("cell_type")] = QStringLiteral("raw");

    const auto result = tool->execute(params);
    ASSERT_TRUE(result.success) << result.error.toStdString();

    const QJsonArray cells =
        loadCells(root, QStringLiteral("test.ipynb"));
    ASSERT_EQ(cells.size(), 3);
    EXPECT_EQ(cells.at(2).toObject()
                  .value(QStringLiteral("cell_type"))
                  .toString(),
              QStringLiteral("raw"));
}

TEST_F(NotebookEditToolTest, PathParameterNotString)
{
    QJsonObject params;
    params[QStringLiteral("path")] = 42;
    params[QStringLiteral("action")] = QStringLiteral("list_cells");

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}

TEST_F(NotebookEditToolTest, ActionParameterNotString)
{
    QJsonObject params;
    params[QStringLiteral("path")] = QStringLiteral("test.ipynb");
    params[QStringLiteral("action")] = 42;

    const auto result = tool->execute(params);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, INVALID_PARAMS);
}
