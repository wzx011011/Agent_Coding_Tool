#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <mutex>

namespace act::harness
{

enum class MemoryType
{
    User,      // User profile, preferences, knowledge level
    Project,   // Project context, architecture, conventions
    Feedback,  // Behavioral preferences (what to do/avoid)
    Reference  // Pointers to external resources (URLs, dashboards)
};

struct MemoryEntry
{
    QString name;          // Unique name within type
    MemoryType type;
    QString description;   // One-line description for indexing
    QString content;       // Full markdown content
    QDateTime updatedAt;
};

/// Manages typed persistent memory entries stored as markdown files.
/// Each entry lives at {memoryDir}/{type}/{name}.md with optional
/// description metadata in the first line.
class MemoryManager
{
public:
    explicit MemoryManager(const QString &memoryDir);

    // CRUD operations
    bool save(MemoryType type, const QString &name,
              const QString &content, const QString &description = {});
    MemoryEntry load(MemoryType type, const QString &name) const;
    bool remove(MemoryType type, const QString &name);
    QList<MemoryEntry> listByType(MemoryType type) const;
    QList<MemoryEntry> listAll() const;

    // Index generation
    QString buildMemoryIndex() const; // Generates MEMORY.md content

    // Directory setup
    bool ensureDirectoryExists();

private:
    QString typeDir(MemoryType type) const;
    static QString memoryTypeToString(MemoryType type);
    static MemoryType stringToMemoryType(const QString &s);
    QString filePath(MemoryType type, const QString &name) const;
    bool writeEntry(const QString &path, const MemoryEntry &entry);
    MemoryEntry readEntry(const QString &path) const;

    QString m_memoryDir; // Base directory: .claude/projects/<hash>/memory/
    mutable std::mutex m_mutex;
};

} // namespace act::harness
