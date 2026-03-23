#pragma once

#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>

#include "infrastructure/interfaces.h"

/// Mock IFileSystem for testing. Uses real QFile operations.
class MockFileSystem : public act::infrastructure::IFileSystem
{
public:
    explicit MockFileSystem(QString workspaceRoot)
        : m_workspaceRoot(QDir::cleanPath(std::move(workspaceRoot)))
    {
    }

    [[nodiscard]] bool readFile(const QString &path,
                                QString &content) const override
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        content = QString::fromUtf8(file.readAll());
        return true;
    }

    bool writeFile(const QString &path,
                   const QString &content) override
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        file.write(content.toUtf8());
        return true;
    }

    [[nodiscard]] QStringList listFiles(
        const QString &dir,
        const QString &pattern) const override
    {
        QDir d(dir);
        return d.entryList({pattern}, QDir::Files);
    }

    [[nodiscard]] QString normalizePath(const QString &path) const override
    {
        if (QDir::isRelativePath(path))
            return QDir::cleanPath(m_workspaceRoot + QLatin1Char('/') + path);
        return QDir::cleanPath(path);
    }

    [[nodiscard]] bool exists(const QString &path) const override
    {
        return QFile::exists(path) || QDir(path).exists();
    }

    bool removeFile(const QString &path) override
    {
        return QFile::remove(path);
    }

private:
    QString m_workspaceRoot;
};
