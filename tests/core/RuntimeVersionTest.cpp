#include <gtest/gtest.h>

#include "core/runtime_version.h"

TEST(RuntimeVersionTest, ReturnsNonEmptyVersion)
{
  EXPECT_FALSE(act::core::runtimeVersion().isEmpty());
}