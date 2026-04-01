#include "harness/tools/notebook_edit_tool.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include <chrono>

#include <spdlog/spdlog.h>

#include "core/error_codes.h"

namespace act::harness
{

NotebookEditTool::NotebookEditTool(act::infrastructure::IFileSystem &fs,
                                   QString workspaceRoot)
    : m_fs(fs)
    , m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
{
}

QString NotebookEditTool::name() const
{
    return QStringLiteral("notebook_edit");
}

QString NotebookEditTool::description() const
{
    return QStringLiteral(
        "Edit Jupyter Notebook (.ipynb) files. Supports reading, adding, "
        "deleting, and editing cells. The file must be in nbformat 4 format.");
}

QJsonObject NotebookEditTool::schema() const
{
    QJsonObject pathProp;
    pathProp[QStringLiteral("type")] = QStringLiteral("string");
    pathProp[QStringLiteral("description")] =
        QStringLiteral("Path to .ipynb file (relative to workspace)");

    QJsonObject actionProp;
    actionProp[QStringLiteral("type")] = QStringLiteral("string");
    {
        QJsonArray actionEnum;
        actionEnum.append(QStringLiteral("read_cell"));
        actionEnum.append(QStringLiteral("add_cell"));
        actionEnum.append(QStringLiteral("delete_cell"));
        actionEnum.append(QStringLiteral("edit_cell"));
        actionEnum.append(QStringLiteral("list_cells"));
        actionProp[QStringLiteral("enum")] = actionEnum;
    }
    actionProp[QStringLiteral("description")] =
        QStringLiteral("Cell operation to perform");

    QJsonObject cellIdProp;
    cellIdProp[QStringLiteral("type")] = QStringLiteral("string");
    cellIdProp[QStringLiteral("description")] =
        QStringLiteral("Cell ID to target (alternative to cell_number)");

    QJsonObject cellNumberProp;
    cellNumberProp[QStringLiteral("type")] = QStringLiteral("integer");
    cellNumberProp[QStringLiteral("description")] =
        QStringLiteral("0-based cell index");

    QJsonObject cellTypeProp;
    cellTypeProp[QStringLiteral("type")] = QStringLiteral("string");
    {
        QJsonArray typeEnum;
        typeEnum.append(QStringLiteral("code"));
        typeEnum.append(QStringLiteral("markdown"));
        typeEnum.append(QStringLiteral("raw"));
        cellTypeProp[QStringLiteral("enum")] = typeEnum;
    }
    cellTypeProp[QStringLiteral("description")] =
        QStringLiteral("Cell type for add_cell (default: code)");

    QJsonObject newSourceProp;
    newSourceProp[QStringLiteral("type")] = QStringLiteral("string");
    newSourceProp[QStringLiteral("description")] =
        QStringLiteral("New cell source content");

    QJsonObject editModeProp;
    editModeProp[QStringLiteral("type")] = QStringLiteral("string");
    {
        QJsonArray modeEnum;
        modeEnum.append(QStringLiteral("replace"));
        modeEnum.append(QStringLiteral("insert"));
        editModeProp[QStringLiteral("enum")] = modeEnum;
    }
    editModeProp[QStringLiteral("description")] =
        QStringLiteral("For edit_cell: replace existing or insert new (default: replace)");

    QJsonObject indexProp;
    indexProp[QStringLiteral("type")] = QStringLiteral("integer");
    indexProp[QStringLiteral("description")] =
        QStringLiteral("Insert position for add_cell (default: append)");

    QJsonObject props;
    props[QStringLiteral("path")] = pathProp;
    props[QStringLiteral("action")] = actionProp;
    props[QStringLiteral("cell_id")] = cellIdProp;
    props[QStringLiteral("cell_number")] = cellNumberProp;
    props[QStringLiteral("cell_type")] = cellTypeProp;
    props[QStringLiteral("new_source")] = newSourceProp;
    props[QStringLiteral("edit_mode")] = editModeProp;
    props[QStringLiteral("index")] = indexProp;

    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("required")] =
        QJsonArray{QStringLiteral("path"), QStringLiteral("action")};
    schema[QStringLiteral("properties")] = props;
    return schema;
}

act::core::ToolResult NotebookEditTool::execute(const QJsonObject &params)
{
    // Validate path parameter
    const auto pathValue = params.value(QStringLiteral("path"));
    if (!pathValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'path' parameter must be a string"));
    }

    const QString rawPath = pathValue.toString();
    const QString normalizedPath = m_fs.normalizePath(rawPath);

    // Validate .ipynb extension
    if (!normalizedPath.endsWith(QStringLiteral(".ipynb")))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Path must end with .ipynb"));
    }

    // Workspace boundary check
    if (!isPathWithinWorkspace(normalizedPath))
    {
        spdlog::warn("NotebookEditTool: path outside workspace: {}",
                     normalizedPath.toStdString());
        return act::core::ToolResult::err(
            act::core::errors::OUTSIDE_WORKSPACE,
            QStringLiteral("Path '%1' is outside the workspace")
                .arg(normalizedPath));
    }

    // Validate action parameter
    const auto actionValue = params.value(QStringLiteral("action"));
    if (!actionValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'action' parameter must be a string"));
    }

    static const QSet<QString> validActions = {
        QStringLiteral("read_cell"), QStringLiteral("add_cell"),
        QStringLiteral("delete_cell"), QStringLiteral("edit_cell"),
        QStringLiteral("list_cells")};

    const QString action = actionValue.toString();
    if (!validActions.contains(action))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Invalid action '%1'. Must be one of: "
                           "read_cell, add_cell, delete_cell, edit_cell, "
                           "list_cells")
                .arg(action));
    }

    // Route to the appropriate cell operation
    if (action == QStringLiteral("read_cell"))
        return readCell(normalizedPath, params);
    if (action == QStringLiteral("add_cell"))
        return addCell(normalizedPath, params);
    if (action == QStringLiteral("delete_cell"))
        return deleteCell(normalizedPath, params);
    if (action == QStringLiteral("edit_cell"))
        return editCell(normalizedPath, params);
    if (action == QStringLiteral("list_cells"))
        return listCells(normalizedPath);

    // Should not reach here
    return act::core::ToolResult::err(
        act::core::errors::INVALID_PARAMS,
        QStringLiteral("Unknown action: %1").arg(action));
}

act::core::PermissionLevel NotebookEditTool::permissionLevel() const
{
    return act::core::PermissionLevel::Write;
}

bool NotebookEditTool::isThreadSafe() const
{
    return false;
}

bool NotebookEditTool::isPathWithinWorkspace(
    const QString &normalizedPath) const
{
    if (!normalizedPath.startsWith(m_workspaceRoot))
        return false;
    if (normalizedPath == m_workspaceRoot)
        return false;
    const QString remainder = normalizedPath.mid(m_workspaceRoot.length());
    if (!remainder.startsWith(QLatin1Char('/')) &&
        !remainder.startsWith(QLatin1Char('\\')))
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// Cell operations
// ---------------------------------------------------------------------------

act::core::ToolResult NotebookEditTool::readCell(const QString &path,
                                                 const QJsonObject &params)
{
    if (!m_fs.exists(path))
    {
        return act::core::ToolResult::err(
            act::core::errors::FILE_NOT_FOUND,
            QStringLiteral("File not found: %1").arg(path));
    }

    QJsonObject nb;
    if (!loadNotebook(path, nb))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Failed to load notebook: %1").arg(path));
    }

    const QString cellId =
        params.value(QStringLiteral("cell_id")).toString();
    const int cellNumber =
        params.value(QStringLiteral("cell_number")).toInt(-1);

    const int idx = findCellIndex(nb, cellId, cellNumber);
    if (idx < 0)
    {
        return act::core::ToolResult::err(
            act::core::errors::STRING_NOT_FOUND,
            QStringLiteral("Cell not found (cell_id='%1', cell_number=%2)")
                .arg(cellId)
                .arg(cellNumber));
    }

    const QJsonObject cell =
        nb.value(QStringLiteral("cells")).toArray().at(idx).toObject();

    // Build output from cell source array
    const QJsonArray sourceArr =
        cell.value(QStringLiteral("source")).toArray();
    QString sourceText;
    for (const auto &line : sourceArr)
    {
        sourceText += line.toString();
    }

    QJsonObject metadata;
    metadata[QStringLiteral("path")] = path;
    metadata[QStringLiteral("cell_index")] = idx;
    metadata[QStringLiteral("cell_type")] =
        cell.value(QStringLiteral("cell_type")).toString();
    metadata[QStringLiteral("cell_id")] =
        cell.value(QStringLiteral("id")).toString();

    return act::core::ToolResult::ok(sourceText, metadata);
}

act::core::ToolResult NotebookEditTool::addCell(const QString &path,
                                                const QJsonObject &params)
{
    const auto newSourceValue =
        params.value(QStringLiteral("new_source"));
    if (!newSourceValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("'new_source' is required for add_cell"));
    }

    const QString newSource = newSourceValue.toString();
    if (newSource.size() > kMaxCellSourceSize)
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Cell source exceeds maximum size of %1 bytes")
                .arg(kMaxCellSourceSize));
    }

    const QString cellType =
        params.value(QStringLiteral("cell_type")).toString(
            QStringLiteral("code"));

    QJsonObject nb;
    bool isNew = false;

    if (m_fs.exists(path))
    {
        if (!loadNotebook(path, nb))
        {
            return act::core::ToolResult::err(
                act::core::errors::INVALID_PARAMS,
                QStringLiteral("Failed to load notebook: %1").arg(path));
        }
    }
    else
    {
        // Create a new empty notebook
        nb[QStringLiteral("nbformat")] = 4;
        nb[QStringLiteral("nbformat_minor")] = 5;
        nb[QStringLiteral("metadata")] = QJsonObject();
        nb[QStringLiteral("cells")] = QJsonArray();
        isNew = true;
    }

    const QJsonObject newCell = createCell(cellType, newSource);

    QJsonArray cells = nb.value(QStringLiteral("cells")).toArray();

    // Determine insert position
    const auto indexValue = params.value(QStringLiteral("index"));
    int insertIdx = cells.size(); // default: append
    if (indexValue.isDouble())
    {
        insertIdx = indexValue.toInt();
        if (insertIdx < 0)
            insertIdx = 0;
        if (insertIdx > cells.size())
            insertIdx = cells.size();
    }

    cells.insert(insertIdx, newCell);
    nb[QStringLiteral("cells")] = cells;

    if (!saveNotebook(path, nb))
    {
        return act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Failed to save notebook: %1").arg(path));
    }

    QJsonObject metadata;
    metadata[QStringLiteral("path")] = path;
    metadata[QStringLiteral("cell_index")] = insertIdx;
    metadata[QStringLiteral("cell_id")] =
        newCell.value(QStringLiteral("id")).toString();
    metadata[QStringLiteral("new_file")] = isNew;

    return act::core::ToolResult::ok(
        QStringLiteral("Added %1 cell at index %2 in %3")
            .arg(cellType)
            .arg(insertIdx)
            .arg(path),
        metadata);
}

act::core::ToolResult NotebookEditTool::deleteCell(const QString &path,
                                                   const QJsonObject &params)
{
    if (!m_fs.exists(path))
    {
        return act::core::ToolResult::err(
            act::core::errors::FILE_NOT_FOUND,
            QStringLiteral("File not found: %1").arg(path));
    }

    QJsonObject nb;
    if (!loadNotebook(path, nb))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Failed to load notebook: %1").arg(path));
    }

    const QString cellId =
        params.value(QStringLiteral("cell_id")).toString();
    const int cellNumber =
        params.value(QStringLiteral("cell_number")).toInt(-1);

    const int idx = findCellIndex(nb, cellId, cellNumber);
    if (idx < 0)
    {
        return act::core::ToolResult::err(
            act::core::errors::STRING_NOT_FOUND,
            QStringLiteral("Cell not found (cell_id='%1', cell_number=%2)")
                .arg(cellId)
                .arg(cellNumber));
    }

    QJsonArray cells = nb.value(QStringLiteral("cells")).toArray();
    cells.removeAt(idx);
    nb[QStringLiteral("cells")] = cells;

    if (!saveNotebook(path, nb))
    {
        return act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Failed to save notebook: %1").arg(path));
    }

    QJsonObject metadata;
    metadata[QStringLiteral("path")] = path;
    metadata[QStringLiteral("deleted_index")] = idx;

    return act::core::ToolResult::ok(
        QStringLiteral("Deleted cell at index %1 from %2").arg(idx).arg(path),
        metadata);
}

act::core::ToolResult NotebookEditTool::editCell(const QString &path,
                                                 const QJsonObject &params)
{
    if (!m_fs.exists(path))
    {
        return act::core::ToolResult::err(
            act::core::errors::FILE_NOT_FOUND,
            QStringLiteral("File not found: %1").arg(path));
    }

    QJsonObject nb;
    if (!loadNotebook(path, nb))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Failed to load notebook: %1").arg(path));
    }

    const QString cellId =
        params.value(QStringLiteral("cell_id")).toString();
    const int cellNumber =
        params.value(QStringLiteral("cell_number")).toInt(-1);
    const QString editMode =
        params.value(QStringLiteral("edit_mode")).toString(
            QStringLiteral("replace"));

    if (editMode == QStringLiteral("insert"))
    {
        // Insert mode: add a new cell at the specified position
        const auto newSourceValue =
            params.value(QStringLiteral("new_source"));
        if (!newSourceValue.isString())
        {
            return act::core::ToolResult::err(
                act::core::errors::INVALID_PARAMS,
                QStringLiteral(
                    "'new_source' is required for edit_cell insert mode"));
        }

        const QString newSource = newSourceValue.toString();
        if (newSource.size() > kMaxCellSourceSize)
        {
            return act::core::ToolResult::err(
                act::core::errors::INVALID_PARAMS,
                QStringLiteral(
                    "Cell source exceeds maximum size of %1 bytes")
                    .arg(kMaxCellSourceSize));
        }

        const QString cellType =
            params.value(QStringLiteral("cell_type")).toString(
                QStringLiteral("code"));
        const QJsonObject newCell = createCell(cellType, newSource);

        QJsonArray cells = nb.value(QStringLiteral("cells")).toArray();

        // Insert after the found cell, or at end if not found by id
        int insertIdx = cells.size();
        if (!cellId.isEmpty() || cellNumber >= 0)
        {
            const int idx = findCellIndex(nb, cellId, cellNumber);
            if (idx >= 0)
                insertIdx = idx + 1;
        }

        cells.insert(insertIdx, newCell);
        nb[QStringLiteral("cells")] = cells;

        if (!saveNotebook(path, nb))
        {
            return act::core::ToolResult::err(
                act::core::errors::PERMISSION_DENIED,
                QStringLiteral("Failed to save notebook: %1").arg(path));
        }

        QJsonObject metadata;
        metadata[QStringLiteral("path")] = path;
        metadata[QStringLiteral("cell_index")] = insertIdx;
        metadata[QStringLiteral("cell_id")] =
            newCell.value(QStringLiteral("id")).toString();

        return act::core::ToolResult::ok(
            QStringLiteral("Inserted %1 cell at index %2 in %3")
                .arg(cellType)
                .arg(insertIdx)
                .arg(path),
            metadata);
    }

    // Default: replace mode
    const auto newSourceValue =
        params.value(QStringLiteral("new_source"));
    if (!newSourceValue.isString())
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral(
                "'new_source' is required for edit_cell replace mode"));
    }

    const QString newSource = newSourceValue.toString();
    if (newSource.size() > kMaxCellSourceSize)
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Cell source exceeds maximum size of %1 bytes")
                .arg(kMaxCellSourceSize));
    }

    const int idx = findCellIndex(nb, cellId, cellNumber);
    if (idx < 0)
    {
        return act::core::ToolResult::err(
            act::core::errors::STRING_NOT_FOUND,
            QStringLiteral("Cell not found (cell_id='%1', cell_number=%2)")
                .arg(cellId)
                .arg(cellNumber));
    }

    QJsonArray cells = nb.value(QStringLiteral("cells")).toArray();
    QJsonObject cell = cells.at(idx).toObject();

    // Convert source string to array of lines
    QJsonArray sourceArr;
    const QStringList lines = newSource.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i)
    {
        if (i < lines.size() - 1)
            sourceArr.append(lines.at(i) + QLatin1Char('\n'));
        else if (!lines.at(i).isEmpty())
            sourceArr.append(lines.at(i));
    }
    cell[QStringLiteral("source")] = sourceArr;

    cells.replace(idx, cell);
    nb[QStringLiteral("cells")] = cells;

    if (!saveNotebook(path, nb))
    {
        return act::core::ToolResult::err(
            act::core::errors::PERMISSION_DENIED,
            QStringLiteral("Failed to save notebook: %1").arg(path));
    }

    QJsonObject metadata;
    metadata[QStringLiteral("path")] = path;
    metadata[QStringLiteral("cell_index")] = idx;
    metadata[QStringLiteral("cell_id")] =
        cell.value(QStringLiteral("id")).toString();

    return act::core::ToolResult::ok(
        QStringLiteral("Edited cell at index %1 in %2").arg(idx).arg(path),
        metadata);
}

act::core::ToolResult NotebookEditTool::listCells(const QString &path)
{
    if (!m_fs.exists(path))
    {
        return act::core::ToolResult::err(
            act::core::errors::FILE_NOT_FOUND,
            QStringLiteral("File not found: %1").arg(path));
    }

    QJsonObject nb;
    if (!loadNotebook(path, nb))
    {
        return act::core::ToolResult::err(
            act::core::errors::INVALID_PARAMS,
            QStringLiteral("Failed to load notebook: %1").arg(path));
    }

    const QJsonArray cells = nb.value(QStringLiteral("cells")).toArray();

    QString output;
    for (int i = 0; i < cells.size(); ++i)
    {
        const QJsonObject cell = cells.at(i).toObject();
        const QString type =
            cell.value(QStringLiteral("cell_type")).toString();
        const QString id =
            cell.value(QStringLiteral("id")).toString();

        // Get first line of source as preview
        const QJsonArray sourceArr =
            cell.value(QStringLiteral("source")).toArray();
        QString firstLine;
        if (!sourceArr.isEmpty())
        {
            firstLine = sourceArr.at(0).toString();
            // Remove trailing newline for display
            if (firstLine.endsWith(QLatin1Char('\n')))
                firstLine.chop(1);
            if (firstLine.length() > 80)
                firstLine = firstLine.left(77) + QStringLiteral("...");
        }

        output += QStringLiteral("[%1] %2 (id=%3): %4\n")
                      .arg(i)
                      .arg(type, id, firstLine);
    }

    if (output.isEmpty())
        output = QStringLiteral("(no cells)\n");

    QJsonObject metadata;
    metadata[QStringLiteral("path")] = path;
    metadata[QStringLiteral("cell_count")] = cells.size();

    return act::core::ToolResult::ok(output, metadata);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool NotebookEditTool::loadNotebook(const QString &path,
                                    QJsonObject &nb) const
{
    QString content;
    if (!m_fs.readFile(path, content))
        return false;

    QJsonParseError err;
    const QJsonDocument doc =
        QJsonDocument::fromJson(content.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError)
    {
        spdlog::warn("NotebookEditTool: JSON parse error in {}: {}",
                     path.toStdString(), err.errorString().toStdString());
        return false;
    }

    if (!doc.isObject())
        return false;

    nb = doc.object();

    // Validate nbformat == 4
    const int nbformat =
        nb.value(QStringLiteral("nbformat")).toInt(-1);
    if (nbformat != 4)
    {
        spdlog::warn("NotebookEditTool: unsupported nbformat {} in {}",
                     nbformat, path.toStdString());
        return false;
    }

    return true;
}

bool NotebookEditTool::saveNotebook(const QString &path,
                                    const QJsonObject &nb) const
{
    const QJsonDocument doc(nb);
    const QByteArray data = doc.toJson(QJsonDocument::Indented);
    return m_fs.writeFile(path, QString::fromUtf8(data));
}

int NotebookEditTool::findCellIndex(const QJsonObject &nb,
                                    const QString &cellId,
                                    int cellNumber) const
{
    const QJsonArray cells = nb.value(QStringLiteral("cells")).toArray();

    if (!cellId.isEmpty())
    {
        for (int i = 0; i < cells.size(); ++i)
        {
            const QString id =
                cells.at(i).toObject().value(QStringLiteral("id")).toString();
            if (id == cellId)
                return i;
        }
        return -1;
    }

    if (cellNumber >= 0 && cellNumber < cells.size())
        return cellNumber;

    return -1;
}

QJsonObject NotebookEditTool::createCell(const QString &type,
                                         const QString &source) const
{
    QJsonObject cell;
    cell[QStringLiteral("cell_type")] = type;
    cell[QStringLiteral("id")] =
        QString::fromStdString(
            std::to_string(std::hash<std::string>{}(
                source.toStdString() +
                std::to_string(
                    std::chrono::steady_clock::now()
                        .time_since_epoch()
                        .count()))));

    cell[QStringLiteral("metadata")] = QJsonObject();

    // Convert source string to array of lines
    QJsonArray sourceArr;
    const QStringList lines = source.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i)
    {
        if (i < lines.size() - 1)
            sourceArr.append(lines.at(i) + QLatin1Char('\n'));
        else if (!lines.at(i).isEmpty())
            sourceArr.append(lines.at(i));
    }
    cell[QStringLiteral("source")] = sourceArr;

    // Code cells need execution_count and outputs
    if (type == QStringLiteral("code"))
    {
        cell[QStringLiteral("execution_count")] =
            QJsonValue::Null;
        cell[QStringLiteral("outputs")] = QJsonArray();
    }

    return cell;
}

} // namespace act::harness
