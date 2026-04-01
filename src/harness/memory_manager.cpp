#include "harness/memory_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <spdlog/spdlog.h>

namespace act::harness
{

// ---- Helpers ----

QString MemoryManager::memoryTypeToString(MemoryType type)
{
    switch (type)
    {
    case MemoryType::User:
        return QStringLiteral("user");
    case MemoryType::Project:
        return QStringLiteral("project");
    case MemoryType::Feedback:
        return QStringLiteral("feedback");
    case MemoryType::Reference:
        return QStringLiteral("reference");
    }
    return QStringLiteral("unknown");
}

MemoryType MemoryManager::stringToMemoryType(const QString &s)
{
    if (s == QStringLiteral("user"))
        return MemoryType::User;
    if (s == QStringLiteral("project"))
        return MemoryType::Project;
    if (s == QStringLiteral("feedback"))
        return MemoryType::Feedback;
    if (s == QStringLiteral("reference"))
        return MemoryType::Reference;
    return MemoryType::User; // Default fallback
}

QString MemoryManager::typeDir(MemoryType type) const
{
    return m_memoryDir + QLatin1Char('/') + memoryTypeToString(type);
}

QString MemoryManager::filePath(MemoryType type, const QString &name) const
{
    return typeDir(type) + QLatin1Char('/') + name + QStringLiteral(".md");
}

// ---- Public ----

MemoryManager::MemoryManager(const QString &memoryDir)
    : m_memoryDir(memoryDir)
{
}

bool MemoryManager::ensureDirectoryExists()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QDir dir(m_memoryDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
    {
        spdlog::error("MemoryManager: failed to create memory dir: {}",
                      m_memoryDir.toStdString());
        return false;
    }

    for (int i = 0; i < 4; ++i)
    {
        const auto type = static_cast<MemoryType>(i);
        QDir td(typeDir(type));
        if (!td.exists() && !td.mkpath(QStringLiteral(".")))
        {
            spdlog::error("MemoryManager: failed to create type dir: {}",
                          td.path().toStdString());
            return false;
        }
    }

    return true;
}

bool MemoryManager::save(MemoryType type, const QString &name,
                         const QString &content, const QString &description)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (name.isEmpty())
    {
        spdlog::warn("MemoryManager::save: empty name");
        return false;
    }

    const QString dir = typeDir(type);
    QDir d(dir);
    if (!d.exists() && !d.mkpath(QStringLiteral(".")))
    {
        spdlog::error("MemoryManager::save: cannot create dir: {}",
                      dir.toStdString());
        return false;
    }

    MemoryEntry entry;
    entry.name = name;
    entry.type = type;
    entry.description = description;
    entry.content = content;
    entry.updatedAt = QDateTime::currentDateTime();

    const QString path = filePath(type, name);
    if (!writeEntry(path, entry))
    {
        spdlog::error("MemoryManager::save: failed to write: {}",
                      path.toStdString());
        return false;
    }

    spdlog::debug("MemoryManager::save: wrote {}/{}",
                  memoryTypeToString(type).toStdString(),
                  name.toStdString());
    return true;
}

MemoryEntry MemoryManager::load(MemoryType type, const QString &name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const QString path = filePath(type, name);
    if (!QFile::exists(path))
    {
        spdlog::debug("MemoryManager::load: not found: {}",
                      path.toStdString());
        return {};
    }

    return readEntry(path);
}

bool MemoryManager::remove(MemoryType type, const QString &name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const QString path = filePath(type, name);
    if (!QFile::exists(path))
    {
        spdlog::debug("MemoryManager::remove: not found: {}",
                      path.toStdString());
        return false;
    }

    if (!QFile::remove(path))
    {
        spdlog::error("MemoryManager::remove: failed to delete: {}",
                      path.toStdString());
        return false;
    }

    spdlog::debug("MemoryManager::remove: deleted {}/{}",
                  memoryTypeToString(type).toStdString(),
                  name.toStdString());
    return true;
}

QList<MemoryEntry> MemoryManager::listByType(MemoryType type) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QList<MemoryEntry> entries;
    const QString dir = typeDir(type);
    QDir d(dir);
    if (!d.exists())
        return entries;

    const QStringList files =
        d.entryList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
    for (const QString &file : files)
    {
        const QString baseName = file.left(file.length() - 3); // strip ".md"
        const QString path = dir + QLatin1Char('/') + file;
        MemoryEntry entry = readEntry(path);
        if (!entry.name.isEmpty())
            entries.append(entry);
    }

    return entries;
}

QList<MemoryEntry> MemoryManager::listAll() const
{
    QList<MemoryEntry> all;
    for (int i = 0; i < 4; ++i)
    {
        const auto type = static_cast<MemoryType>(i);
        all.append(listByType(type));
    }
    return all;
}

QString MemoryManager::buildMemoryIndex() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QString index;
    index += QStringLiteral("# Memory Index\n\n");

    bool hasAny = false;
    for (int i = 0; i < 4; ++i)
    {
        const auto type = static_cast<MemoryType>(i);
        const QString typeStr = memoryTypeToString(type);
        const QString dir = typeDir(type);
        QDir d(dir);
        if (!d.exists())
            continue;

        const QStringList files =
            d.entryList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
        if (files.isEmpty())
            continue;

        hasAny = true;
        index += QStringLiteral("## %1\n\n").arg(typeStr);

        for (const QString &file : files)
        {
            const QString baseName =
                file.left(file.length() - 3); // strip ".md"
            const QString path = dir + QLatin1Char('/') + file;
            const MemoryEntry entry = readEntry(path);

            QString desc = entry.description.isEmpty()
                               ? QStringLiteral("(no description)")
                               : entry.description;

            index +=
                QStringLiteral("- [%1](%2/%3) — %4\n")
                    .arg(baseName, typeStr, file, desc);
        }
        index += QLatin1Char('\n');
    }

    if (!hasAny)
    {
        index += QStringLiteral("No memory entries yet.\n");
    }

    return index;
}

// ---- Private I/O ----

bool MemoryManager::writeEntry(const QString &path, const MemoryEntry &entry)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        spdlog::error("MemoryManager::writeEntry: cannot open: {}",
                      path.toStdString());
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    // Write description metadata as HTML comment on first line (if present)
    if (!entry.description.isEmpty())
    {
        stream << QStringLiteral("<!-- description: %1 -->\n").arg(entry.description);
    }

    // Write updatedAt as second-line metadata
    stream << QStringLiteral("<!-- updated: %1 -->\n")
                  .arg(entry.updatedAt.toString(Qt::ISODate));

    // Write content
    if (!entry.content.isEmpty())
    {
        // Ensure a blank line separates metadata from content
        if (!entry.content.startsWith(QLatin1Char('\n')))
            stream << QLatin1Char('\n');
        stream << entry.content;
    }

    stream << QLatin1Char('\n');
    return true;
}

MemoryEntry MemoryManager::readEntry(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        spdlog::error("MemoryManager::readEntry: cannot open: {}",
                      path.toStdString());
        return {};
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString all = stream.readAll();

    // Parse metadata from the first lines
    MemoryEntry entry;
    entry.type = stringToMemoryType(
        QFileInfo(path).dir().dirName());

    const QString fileName = QFileInfo(path).fileName();
    entry.name = fileName.left(fileName.length() - 3); // strip ".md"

    QString content = all;

    // Extract description from <!-- description: ... -->
    static const QString descPrefix = QStringLiteral("<!-- description: ");
    static const QString descSuffix = QStringLiteral(" -->");
    if (content.startsWith(descPrefix))
    {
        const int end = content.indexOf(descSuffix, descPrefix.length());
        if (end >= 0)
        {
            entry.description =
                content.mid(descPrefix.length(), end - descPrefix.length());
            content.remove(0, end + descSuffix.length());
        }
    }

    // Extract updated timestamp from <!-- updated: ... -->
    static const QString updatedPrefix = QStringLiteral("<!-- updated: ");
    static const QString updatedSuffix = QStringLiteral(" -->");
    content = content.trimmed();
    if (content.startsWith(updatedPrefix))
    {
        const int end = content.indexOf(updatedSuffix, updatedPrefix.length());
        if (end >= 0)
        {
            const QString ts =
                content.mid(updatedPrefix.length(), end - updatedPrefix.length());
            entry.updatedAt = QDateTime::fromString(ts, Qt::ISODate);
            content.remove(0, end + updatedSuffix.length());
        }
    }

    entry.content = content.trimmed();
    return entry;
}

} // namespace act::harness
