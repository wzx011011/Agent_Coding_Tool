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
    // INetwork inherits QObject — verify through pointer conversion
    INetwork *net = nullptr;
    QObject *obj = net;
    ASSERT_EQ(obj, nullptr);
}

TEST(IProcessTest, InheritsQObject)
{
    IProcess *proc = nullptr;
    QObject *obj = proc;
    ASSERT_EQ(obj, nullptr);
}

TEST(ITerminalTest, InterfaceIsDefined)
{
    ITerminal *term = nullptr;
    ASSERT_EQ(term, nullptr);
}
