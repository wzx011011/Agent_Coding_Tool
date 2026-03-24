#include "framework/patch_transaction.h"

#include <QDir>
#include <QStringList>

#include <spdlog/spdlog.h>

namespace act::framework
{

void PatchTransaction::addPatch(const QString &filePath,
                                 const QString &originalContent,
                                 const QString &newContent)
{
    PatchEntry entry;
    entry.filePath = filePath;
    entry.originalContent = originalContent;
    entry.newContent = newContent;
    entry.accepted = false;

    m_pathIndex[filePath] = m_patches.size();
    m_patches.append(entry);

    spdlog::debug("PatchTransaction: added patch for '{}'",
                  filePath.toStdString());
}

bool PatchTransaction::accept(const QString &filePath)
{
    auto it = m_pathIndex.find(filePath);
    if (it == m_pathIndex.end())
        return false;

    m_patches[it.value()].accepted = true;
    spdlog::info("PatchTransaction: accepted '{}'",
                 filePath.toStdString());
    return true;
}

bool PatchTransaction::reject(const QString &filePath)
{
    auto it = m_pathIndex.find(filePath);
    if (it == m_pathIndex.end())
        return false;

    m_patches[it.value()].accepted = false;
    // Mark as rejected by removing the patch
    spdlog::info("PatchTransaction: rejected '{}'",
                 filePath.toStdString());
    return true;
}

void PatchTransaction::acceptAll()
{
    for (auto &patch : m_patches)
        patch.accepted = true;
}

void PatchTransaction::rejectAll()
{
    for (auto &patch : m_patches)
        patch.accepted = false;
}

QString PatchTransaction::diffForFile(const QString &filePath) const
{
    auto it = m_pathIndex.find(filePath);
    if (it == m_pathIndex.end())
        return {};

    const auto &entry = m_patches[it.value()];
    return unifiedDiff(entry.filePath, entry.originalContent,
                       entry.newContent);
}

QString PatchTransaction::diffAll() const
{
    QString result;
    for (const auto &patch : m_patches)
    {
        result += unifiedDiff(patch.filePath, patch.originalContent,
                              patch.newContent);
        result += QStringLiteral("\n");
    }
    return result;
}

QStringList PatchTransaction::filePaths() const
{
    return m_pathIndex.keys();
}

int PatchTransaction::size() const
{
    return m_patches.size();
}

int PatchTransaction::acceptedCount() const
{
    int count = 0;
    for (const auto &patch : m_patches)
    {
        if (patch.accepted)
            ++count;
    }
    return count;
}

int PatchTransaction::rejectedCount() const
{
    int count = 0;
    for (const auto &patch : m_patches)
    {
        if (!patch.accepted && !patch.newContent.isNull())
            ++count;
    }
    return count;
}

int PatchTransaction::pendingCount() const
{
    int count = 0;
    for (const auto &patch : m_patches)
    {
        if (patch.newContent.isNull())
            ++count;
    }
    return count;
}

bool PatchTransaction::hasFile(const QString &filePath) const
{
    return m_pathIndex.contains(filePath);
}

QString PatchTransaction::newContentFor(const QString &filePath) const
{
    auto it = m_pathIndex.find(filePath);
    if (it == m_pathIndex.end())
        return {};

    if (m_patches[it.value()].accepted)
        return m_patches[it.value()].newContent;
    return {};
}

const PatchEntry *PatchTransaction::entry(const QString &filePath) const
{
    auto it = m_pathIndex.find(filePath);
    if (it == m_pathIndex.end())
        return nullptr;
    return &m_patches[it.value()];
}

bool PatchTransaction::allDecided() const
{
    for (const auto &patch : m_patches)
    {
        if (patch.newContent.isNull())
            return false; // Still undecided
    }
    return true;
}

QString PatchTransaction::unifiedDiff(const QString &filePath,
                                        const QString &oldContent,
                                        const QString &newContent)
{
    if (oldContent == newContent)
        return QStringLiteral("  (no changes)\n");

    QStringList oldLines = oldContent.split('\n');
    QStringList newLines = newContent.split('\n');

    QString result = QStringLiteral("--- a/%1\n+++ b/%1\n").arg(filePath);

    // Simple line-by-line diff (unified format)
    int oldIdx = 0;
    int newIdx = 0;

    // Find common prefix
    while (oldIdx < oldLines.size() && newIdx < newLines.size() &&
           oldLines[oldIdx] == newLines[newIdx])
    {
        ++oldIdx;
        ++newIdx;
    }

    // Find common suffix
    int oldEnd = oldLines.size() - 1;
    int newEnd = newLines.size() - 1;
    while (oldEnd >= oldIdx && newEnd >= newIdx &&
           oldLines[oldEnd] == newLines[newEnd])
    {
        --oldEnd;
        --newEnd;
    }

    // Header line with line numbers
    int contextLines = 1;
    int oldStart = qMax(1, oldIdx - contextLines + 1);
    int newStart = qMax(1, newIdx - contextLines + 1);
    int oldCount = qMin(oldLines.size(), oldEnd + contextLines + 1) -
                  oldStart + 1;
    int newCount = qMin(newLines.size(), newEnd + contextLines + 1) -
                  newStart + 1;

    result += QStringLiteral("@@ -%1,%2 +%3,%4 @@\n")
                   .arg(oldStart)
                   .arg(qMax(0, oldCount))
                   .arg(newStart)
                   .arg(qMax(0, newCount));

    // Context before change
    for (int i = oldStart - 1; i < oldIdx; ++i)
    {
        if (i >= 0 && i < oldLines.size())
            result += QStringLiteral(" ") + oldLines[i] + QStringLiteral("\n");
    }

    // Removed lines
    for (int i = oldIdx; i <= oldEnd; ++i)
    {
        if (i < oldLines.size())
            result += QStringLiteral("-") + oldLines[i] + QStringLiteral("\n");
    }

    // Added lines
    for (int i = newIdx; i <= newEnd; ++i)
    {
        if (i < newLines.size())
            result += QStringLiteral("+") + newLines[i] + QStringLiteral("\n");
    }

    // Context after change
    for (int i = oldEnd + 1;
         i <= qMin(oldLines.size() - 1, oldEnd + contextLines); ++i)
    {
        result += QStringLiteral(" ") + oldLines[i] + QStringLiteral("\n");
    }

    return result;
}

} // namespace act::framework
