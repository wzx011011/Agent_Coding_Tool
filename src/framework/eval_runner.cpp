#include "framework/eval_runner.h"

#include <QJsonArray>
#include <QJsonObject>

#include <spdlog/spdlog.h>

namespace act::framework
{

void EvalRunner::addTest(const QString &name,
                          const QString &description)
{
    m_tests.append({name, description});
    spdlog::debug("EvalRunner: added test '{}'", name.toStdString());
}

QString EvalRunner::run(TestExecutor executor)
{
    m_results.clear();

    spdlog::info("EvalRunner: running {} tests", m_tests.size());

    for (const auto &test : m_tests)
    {
        EvalResult result;
        result.testName = test.name;

        if (!executor)
        {
            result.skipped = true;
            m_results.append(result);
            continue;
        }

        // Execute with timing (simplified — uses wall clock)
        auto start = std::chrono::steady_clock::now();

        QString errorMsg;
        bool passed = executor(test.name, errorMsg);
        auto end = std::chrono::steady_clock::now();

        result.passed = passed;
        result.durationMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start)
                .count());
        result.errorMessage = errorMsg;

        m_results.append(result);

        if (passed)
            spdlog::info("  [PASS] {} ({}ms)", test.name.toStdString(),
                         result.durationMs);
        else
            spdlog::warn("  [FAIL] {} ({}ms): {}",
                        test.name.toStdString(), result.durationMs,
                        errorMsg.toStdString());
    }

    auto report = this->report();
    QJsonDocument doc(report);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

int EvalRunner::passCount() const
{
    int count = 0;
    for (const auto &r : m_results)
        if (r.passed) ++count;
    return count;
}

int EvalRunner::failCount() const
{
    int count = 0;
    for (const auto &r : m_results)
        if (!r.passed && !r.skipped) ++count;
    return count;
}

int EvalRunner::totalCount() const
{
    return m_results.size();
}

QJsonObject EvalRunner::report() const
{
    QJsonObject root;

    root[QStringLiteral("total")] = m_results.size();
    root[QStringLiteral("passed")] = passCount();
    root[QStringLiteral("failed")] = failCount();

    QJsonArray resultsArr;
    for (const auto &r : m_results)
    {
        QJsonObject obj;
        obj[QStringLiteral("name")] = r.testName;
        obj[QStringLiteral("passed")] = r.passed;
        obj[QStringLiteral("skipped")] = r.skipped;
        obj[QStringLiteral("timed_out")] = r.timedOut;
        obj[QStringLiteral("duration_ms")] = r.durationMs;

        if (!r.errorMessage.isEmpty())
            obj[QStringLiteral("error")] = r.errorMessage;

        resultsArr.append(obj);
    }
    root[QStringLiteral("results")] = resultsArr;

    return root;
}

} // namespace act::framework
