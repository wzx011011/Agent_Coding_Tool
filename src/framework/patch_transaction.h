#pragma once

#include <QList>
#include <QMap>
#include <QString>

namespace act::framework
{

/// Represents a single file modification within a transaction.
struct PatchEntry
{
    QString filePath;      // Relative path within workspace
    QString originalContent; // Content before modification
    QString newContent;     // Content after modification
    bool accepted = false;  // Whether user accepted this change
};

/// Manages a batch of file modifications as a transaction.
/// Supports preview (diff), accept/reject per file, and rollback.
class PatchTransaction
{
public:
    /// Add a file modification to the transaction.
    void addPatch(const QString &filePath,
                   const QString &originalContent,
                   const QString &newContent);

    /// Accept a specific patch by file path.
    [[nodiscard]] bool accept(const QString &filePath);

    /// Reject a specific patch by file path.
    [[nodiscard]] bool reject(const QString &filePath);

    /// Accept all patches.
    void acceptAll();

    /// Reject all patches.
    void rejectAll();

    /// Generate a unified diff for a specific file.
    [[nodiscard]] QString diffForFile(const QString &filePath) const;

    /// Generate unified diffs for all pending (undecided) patches.
    [[nodiscard]] QString diffAll() const;

    /// Get the list of file paths in this transaction.
    [[nodiscard]] QStringList filePaths() const;

    /// Get the number of patches.
    [[nodiscard]] int size() const;

    /// Get the number of accepted patches.
    [[nodiscard]] int acceptedCount() const;

    /// Get the number of rejected patches.
    [[nodiscard]] int rejectedCount() const;

    /// Get pending (undecided) patches.
    [[nodiscard]] int pendingCount() const;

    /// Check if a file path is in this transaction.
    [[nodiscard]] bool hasFile(const QString &filePath) const;

    /// Get the new content for an accepted patch.
    [[nodiscard]] QString newContentFor(
        const QString &filePath) const;

    /// Get a specific patch entry (const).
    [[nodiscard]] const PatchEntry *entry(
        const QString &filePath) const;

    /// Check if all patches have been decided.
    [[nodiscard]] bool allDecided() const;

private:
    /// Generate unified diff between two strings.
    [[nodiscard]] static QString unifiedDiff(
        const QString &filePath,
        const QString &oldContent,
        const QString &newContent);

    QList<PatchEntry> m_patches;
    QMap<QString, int> m_pathIndex; // Path -> index in m_patches
};

} // namespace act::framework
