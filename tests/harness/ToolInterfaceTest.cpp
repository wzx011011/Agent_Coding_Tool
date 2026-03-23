#include <gtest/gtest.h>

#include "harness/interfaces.h"

using namespace act::harness;

// Compile-only tests: verify ITool interface is well-formed

TEST(IToolTest, InterfaceIsDefined)
{
    ITool *tool = nullptr;
    ASSERT_EQ(tool, nullptr);
}
