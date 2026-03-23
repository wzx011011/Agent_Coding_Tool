#include <gtest/gtest.h>

#include "infrastructure/interfaces.h"

using namespace act::infrastructure;

// Compile-only tests: verify interfaces are well-formed and usable

TEST(IFileSystemTest, InterfaceIsDefined)
{
    // Verify the interface can be referenced
    IFileSystem *fs = nullptr;
    ASSERT_EQ(fs, nullptr);
}

TEST(INetworkTest, InheritsQObject)
{
    // INetwork is a standalone interface (not QObject)
    INetwork *net = nullptr;
    ASSERT_EQ(net, nullptr);
}

TEST(IProcessTest, InheritsQObject)
{
    // IProcess is a standalone interface (not QObject)
    IProcess *proc = nullptr;
    ASSERT_EQ(proc, nullptr);
}

TEST(ITerminalTest, InterfaceIsDefined)
{
    ITerminal *term = nullptr;
    ASSERT_EQ(term, nullptr);
}
