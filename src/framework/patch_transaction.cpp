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
    entry.decision = PatchEntry::Decision::Pending;

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

    m_patches[it.value()].decision = PatchEntry::Decision::Accepted;
    spdlog::info("PatchTransaction: accepted '{}'",
                 filePath.toStdString());
    return true;
}

bool PatchTransaction::reject(const QString &filePath)
{
    auto it = m_pathIndex.find(filePath);
    if (it == m_pathIndex.end())
        return false;

    m_patches[it.value()].decision = PatchEntry::Decision::Rejected;
    spdlog::info("PatchTransaction: rejected '{}'",
                 filePath.toStdString());
    return true;
}

void PatchTransaction::acceptAll()
{
    for (auto &patch : m_patches)
        patch.decision = PatchEntry::Decision::Accepted;
}

void PatchTransaction::rejectAll()
{
    for (auto &patch : m_patches)
        patch.decision = PatchEntry::Decision::Rejected;
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
        if (patch.decision == PatchEntry::Decision::Accepted)
            ++count;
    }
    return count;
}

int PatchTransaction::rejectedCount() const
{
    int count = 0;
    for (const auto &patch : m_patches)
    {
        if (patch.decision == PatchEntry::Decision::Rejected)
            ++count;
    }
    return count;
}

int PatchTransaction::pendingCount() const
{
    int count = 0;
    for (const auto &patch : m_patches)
    {
        if (patch.decision == PatchEntry::Decision::Pending)
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

    if (m_patches[it.value()].decision == PatchEntry::Decision::Accepted)
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
        if (patch.decision == PatchEntry::Decision::Pending)
            return false;
    }
    return true;
}

// --- v1: Multi-file batch operations ---

QMap<QString, QString> PatchTransaction::acceptedPatches() const
{
    QMap<QString, QString> result;
    for (const auto &patch : m_patches)
    {
        if (patch.decision == PatchEntry::Decision::Accepted)
            result[patch.filePath] = patch.newContent;
    }
    return result;
}

QStringList PatchTransaction::rejectedPaths() const
{
    QStringList result;
    for (const auto &patch : m_patches)
    {
        if (patch.decision == PatchEntry::Decision::Rejected)
            result.append(patch.filePath);
    }
    return result;
}

QString PatchTransaction::batchSummary() const
{
    int accepted = acceptedCount();
    int rejected = rejectedCount();
    int pending = pendingCount();

    return QStringLiteral("PatchTransaction: %1 files — "
                         "%2 accepted, %3 rejected, %4 pending")
        .arg(m_patches.size())
        .arg(accepted)
        .arg(rejected)
        .arg(pending);
}

void PatchTransaction::applyPartialFailure(
    const QStringList &failedPaths)
{
    for (const auto &path : failedPaths)
    {
        auto it = m_pathIndex.find(path);
        if (it != m_pathIndex.end())
        {
            m_patches[it.value()].decision =
                PatchEntry::Decision::Rejected;
            spdlog::warn("PatchTransaction: partial failure for '{}'",
                         path.toStdString());
        }
    }
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

    for (int i = oldStart - 1; i < oldIdx; ++i)
    {
        if (i >= 0 && i < oldLines.size())
            result += QStringLiteral(" ") + oldLines[i] + QStringLiteral("\n");
    }

    for (int i = oldIdx; i <= oldEnd; ++i)
    {
        if (i < oldLines.size())
            result += QStringLiteral("-") + oldLines[i] + QStringLiteral("\n");
    }

    for (int i = newIdx; i <= newEnd; ++i)
    {
        if (i < newLines.size())
            result += QStringLiteral("+") + newLines[i] + QStringLiteral("\n");
    }

    for (int i = oldEnd + 1;
         i <= qMin(oldLines.size() - 1, oldEnd + contextLines); ++i)
    {
        result += QStringLiteral(" ") + oldLines[i] + QStringLiteral("\n");
    }

    return result;
}

} // namespace act::framework
