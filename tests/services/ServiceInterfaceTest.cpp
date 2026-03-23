#include <gtest/gtest.h>

#include "services/interfaces.h"

using namespace act::services;

// Compile-only tests: verify service interfaces are well-formed

TEST(IConfigManagerTest, InterfaceIsDefined)
{
    IConfigManager *cfg = nullptr;
    ASSERT_EQ(cfg, nullptr);
}

TEST(IAIEngineTest, InterfaceIsDefined)
{
    IAIEngine *engine = nullptr;
    ASSERT_EQ(engine, nullptr);
}

TEST(IProjectManagerTest, InterfaceIsDefined)
{
    IProjectManager *pm = nullptr;
    ASSERT_EQ(pm, nullptr);
}

TEST(ICodeAnalyzerTest, InterfaceIsDefined)
{
    ICodeAnalyzer *analyzer = nullptr;
    ASSERT_EQ(analyzer, nullptr);
}
