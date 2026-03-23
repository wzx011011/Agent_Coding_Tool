#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace act::infrastructure
{

class IFileSystem
{
public:
    virtual ~IFileSystem() = default;

    [[nodiscard]] virtual bool readFile(const QString &path,
                                        QString &content) const = 0;
    virtual bool writeFile(const QString &path,
                           const QString &content) = 0;
    [[nodiscard]] virtual QStringList listFiles(const QString &dir,
                                                const QString &pattern = QStringLiteral("*")) const = 0;
    [[nodiscard]] virtual QString normalizePath(const QString &path) const = 0;
    [[nodiscard]] virtual bool exists(const QString &path) const = 0;
    virtual bool removeFile(const QString &path) = 0;
};

class INetwork
{
public:
    virtual ~INetwork() = default;

    virtual void httpRequest(const QJsonObject &request,
                             std::function<void(QJsonObject)> callback) = 0;
    virtual void sseRequest(const QJsonObject &request,
                            std::function<void(QString)> onToken,
                            std::function<void(QJsonObject)> onComplete,
                            std::function<void(QString)> onError) = 0;
};

class IProcess
{
public:
    virtual ~IProcess() = default;

    virtual void execute(const QString &command,
                         const QStringList &args,
                         std::function<void(int, QString)> callback,
                         int timeoutMs = 30000) = 0;
    virtual void cancel() = 0;
};

class ITerminal
{
public:
    virtual ~ITerminal() = default;

    virtual void executeInteractive(const QString &command,
                                    std::function<void(QString)> onOutput) = 0;
};

} // namespace act::infrastructure
