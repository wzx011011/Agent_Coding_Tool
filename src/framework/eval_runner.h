#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "framework/task_graph.h"

namespace act::framework
{

/// Represents the result of a single test case execution.
struct EvalResult
{
    QString testName;
    bool passed = false;
    bool skipped = false;
    bool timedOut = false;
    int durationMs = 0;
    QString errorMessage;
};

/// Executes a regression test suite and produces structured reports.
/// v0: Runs test cases via callback and outputs JSON results.
class EvalRunner
{
public:
    /// Definition of a single test case.
    struct TestCase
    {
        QString name;
        QString description;
    };

    /// Callback to execute a test case.
    /// Returns true if passed, false if failed.
    using TestExecutor = std::function<bool(
        const QString &testName, QString &errorMsg)>;

    /// Add a test case to the suite.
    void addTest(const QString &name, const QString &description = {});

    /// Run all tests and collect results.
    /// Returns the JSON report string.
    [[nodiscard]] QString run(TestExecutor executor);

    /// Get results after running.
    [[nodiscard]] const QList<EvalResult> &results() const { return m_results; }

    /// Number of tests that passed.
    [[nodiscard]] int passCount() const;

    /// Number of tests that failed.
    [[nodiscard]] int failCount() const;

    /// Total test count.
    [[nodiscard]] int totalCount() const;

    /// Generate a JSON report from results.
    [[nodiscard]] QJsonObject report() const;

private:
    QList<TestCase> m_tests;
    QList<EvalResult> m_results;
};

} // namespace act::framework
