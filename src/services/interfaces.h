#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>

#include <functional>

#include "core/types.h"

namespace act::services
{

class IConfigManager
{
public:
    virtual ~IConfigManager() = default;

    [[nodiscard]] virtual QString currentModel() const = 0;
    virtual void setModel(const QString &model) = 0;
    [[nodiscard]] virtual QString apiKey(const QString &provider) const = 0;
    virtual void setApiKey(const QString &provider, const QString &key) = 0;

    [[nodiscard]] virtual QString configFilePath() const = 0;
    [[nodiscard]] virtual bool isConfigured() const = 0;
};

class IAIEngine
{
public:
    virtual ~IAIEngine() = default;

    virtual void chat(const QList<act::core::LLMMessage> &messages,
                      std::function<void(act::core::LLMMessage)> onMessage,
                      std::function<void()> onComplete,
                      std::function<void(QString, QString)> onError) = 0;
    virtual void cancel() = 0;
    [[nodiscard]] virtual int estimateTokens(
        const QList<act::core::LLMMessage> &messages) const = 0;
    virtual void setToolDefinitions(const QList<QJsonObject> &tools) = 0;
};

class IProjectManager
{
public:
    virtual ~IProjectManager() = default;

    virtual bool openProject(const QString &path) = 0;
    [[nodiscard]] virtual QStringList listFiles(
        const QString &pattern = QStringLiteral("*")) const = 0;
    [[nodiscard]] virtual QString resolvePath(
        const QString &relativePath) const = 0;
};

class ICodeAnalyzer
{
public:
    virtual ~ICodeAnalyzer() = default;

    [[nodiscard]] virtual QJsonObject buildRepoMap(
        const QStringList &files) const = 0;
    [[nodiscard]] virtual QString getContextForQuery(
        const QString &query,
        int maxTokens) const = 0;
};

} // namespace act::services
