#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "framework/plugin_system.h"
#include "framework/plugin_types.h"

using namespace act::framework;

namespace {

/// Helper to create a plugin directory with a plugin.toml manifest.
void createPluginDir(const QString &parentDir,
                     const QString &name,
                     const QString &tomlContent)
{
    QString pluginDir = parentDir + QLatin1Char('/') + name;
    QDir().mkpath(pluginDir);

    QString filePath = pluginDir + QStringLiteral("/plugin.toml");
    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write(tomlContent.toUtf8());
    file.close();
}

} // anonymous namespace

TEST(PluginSystemTest, DiscoverEmptyDirectory)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    PluginSystem ps(tmpDir.path());
    auto plugins = ps.discoverPlugins();
    EXPECT_TRUE(plugins.isEmpty());
}

TEST(PluginSystemTest, DiscoverNonExistentDirectory)
{
    PluginSystem ps(QStringLiteral("/nonexistent/path"));
    auto plugins = ps.discoverPlugins();
    EXPECT_TRUE(plugins.isEmpty());
}

TEST(PluginSystemTest, DiscoverSinglePlugin)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("code-reviewer"),
                    QStringLiteral(R"(
[plugin]
name = "code-reviewer"
version = "1.0.0"
type = "skill"
description = "Automated code review"
entry_point = "skill.toml"
dependencies = []
)"));

    PluginSystem ps(tmpDir.path());
    auto plugins = ps.discoverPlugins();
    ASSERT_EQ(plugins.size(), 1);
    EXPECT_EQ(plugins[0].name, QStringLiteral("code-reviewer"));
    EXPECT_EQ(plugins[0].version, QStringLiteral("1.0.0"));
    EXPECT_EQ(plugins[0].type, PluginType::Skill);
    EXPECT_EQ(plugins[0].description,
              QStringLiteral("Automated code review"));
    EXPECT_EQ(plugins[0].entryPoint, QStringLiteral("skill.toml"));
}

TEST(PluginSystemTest, DiscoverMultiplePlugins)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("reviewer"),
                    QStringLiteral(R"(
[plugin]
name = "reviewer"
version = "1.0.0"
type = "skill"
description = "Code review"
)"));

    createPluginDir(tmpDir.path(), QStringLiteral("runner"),
                    QStringLiteral(R"(
[plugin]
name = "runner"
version = "2.0.0"
type = "agent"
description = "Test runner"
)"));

    PluginSystem ps(tmpDir.path());
    auto plugins = ps.discoverPlugins();
    EXPECT_EQ(plugins.size(), 2);
}

TEST(PluginSystemTest, DiscoverPluginTypes)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("skill-plugin"),
                    QStringLiteral("[plugin]\nname = \"sp\"\ntype = \"skill\"\n"));
    createPluginDir(tmpDir.path(), QStringLiteral("agent-plugin"),
                    QStringLiteral("[plugin]\nname = \"ap\"\ntype = \"agent\"\n"));
    createPluginDir(tmpDir.path(), QStringLiteral("hook-plugin"),
                    QStringLiteral("[plugin]\nname = \"hp\"\ntype = \"hook\"\n"));
    createPluginDir(tmpDir.path(), QStringLiteral("mcp-plugin"),
                    QStringLiteral("[plugin]\nname = \"mp\"\ntype = \"mcp\"\n"));

    PluginSystem ps(tmpDir.path());
    auto plugins = ps.discoverPlugins();
    ASSERT_EQ(plugins.size(), 4);

    // Find each plugin by name
    QMap<QString, PluginType> typeMap;
    for (const auto &p : plugins)
        typeMap[p.name] = p.type;

    EXPECT_EQ(typeMap[QStringLiteral("sp")], PluginType::Skill);
    EXPECT_EQ(typeMap[QStringLiteral("ap")], PluginType::Agent);
    EXPECT_EQ(typeMap[QStringLiteral("hp")], PluginType::Hook);
    EXPECT_EQ(typeMap[QStringLiteral("mp")], PluginType::MCP);
}

TEST(PluginSystemTest, DefaultVersion)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("noversion"),
                    QStringLiteral("[plugin]\nname = \"noversion\"\n"));

    PluginSystem ps(tmpDir.path());
    auto plugins = ps.discoverPlugins();
    ASSERT_EQ(plugins.size(), 1);
    EXPECT_EQ(plugins[0].version, QStringLiteral("0.1.0"));
}

TEST(PluginSystemTest, LoadPlugin)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("my-plugin"),
                    QStringLiteral(R"(
[plugin]
name = "my-plugin"
version = "1.0.0"
type = "skill"
description = "Test plugin"
)"));

    PluginSystem ps(tmpDir.path());
    EXPECT_TRUE(ps.loadPlugin(QStringLiteral("my-plugin")));
    EXPECT_EQ(ps.loadedPlugins().size(), 1);
    EXPECT_TRUE(ps.loadedPlugins().contains(QStringLiteral("my-plugin")));
}

TEST(PluginSystemTest, LoadNonExistentPlugin)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    PluginSystem ps(tmpDir.path());
    EXPECT_FALSE(ps.loadPlugin(QStringLiteral("nonexistent")));
}

TEST(PluginSystemTest, GetPlugin)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("getter-test"),
                    QStringLiteral(R"(
[plugin]
name = "getter-test"
version = "3.0.0"
type = "hook"
description = "Getter test"
)"));

    PluginSystem ps(tmpDir.path());
    ps.loadPlugin(QStringLiteral("getter-test"));

    auto manifest = ps.getPlugin(QStringLiteral("getter-test"));
    ASSERT_TRUE(manifest.has_value());
    EXPECT_EQ(manifest->name, QStringLiteral("getter-test"));
    EXPECT_EQ(manifest->version, QStringLiteral("3.0.0"));
    EXPECT_EQ(manifest->type, PluginType::Hook);
}

TEST(PluginSystemTest, GetPluginNotFound)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    PluginSystem ps(tmpDir.path());
    auto manifest = ps.getPlugin(QStringLiteral("nope"));
    EXPECT_FALSE(manifest.has_value());
}

TEST(PluginSystemTest, UnloadPlugin)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("unload-test"),
                    QStringLiteral("[plugin]\nname = \"unload-test\"\n"));

    PluginSystem ps(tmpDir.path());
    ps.loadPlugin(QStringLiteral("unload-test"));
    ASSERT_EQ(ps.loadedPlugins().size(), 1);

    EXPECT_TRUE(ps.unloadPlugin(QStringLiteral("unload-test")));
    EXPECT_TRUE(ps.loadedPlugins().isEmpty());
}

TEST(PluginSystemTest, UnloadNonExistentPlugin)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    PluginSystem ps(tmpDir.path());
    EXPECT_FALSE(ps.unloadPlugin(QStringLiteral("nope")));
}

TEST(PluginSystemTest, PluginWithDependencies)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("with-deps"),
                    QStringLiteral(R"(
[plugin]
name = "with-deps"
version = "1.0.0"
type = "skill"
dependencies = ["base-plugin", "utils"]
)"));

    PluginSystem ps(tmpDir.path());
    auto plugins = ps.discoverPlugins();
    ASSERT_EQ(plugins.size(), 1);
    ASSERT_EQ(plugins[0].dependencies.size(), 2);
    EXPECT_EQ(plugins[0].dependencies[0], QStringLiteral("base-plugin"));
    EXPECT_EQ(plugins[0].dependencies[1], QStringLiteral("utils"));
}

TEST(PluginSystemTest, PluginWithConfig)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("with-config"),
                    QStringLiteral(R"(
[plugin]
name = "with-config"
version = "1.0.0"
type = "mcp"

[plugin.config]
server_url = "http://localhost:8080"
timeout = "30"
)"));

    PluginSystem ps(tmpDir.path());
    auto plugins = ps.discoverPlugins();
    ASSERT_EQ(plugins.size(), 1);
    EXPECT_EQ(plugins[0].config.size(), 2);
    EXPECT_EQ(plugins[0].config[QStringLiteral("server_url")],
              QStringLiteral("http://localhost:8080"));
    EXPECT_EQ(plugins[0].config[QStringLiteral("timeout")],
              QStringLiteral("30"));
}

TEST(PluginSystemTest, SkipDirectoryWithoutManifest)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    // Create a subdirectory without plugin.toml
    QDir().mkpath(tmpDir.path() + QStringLiteral("/empty-dir"));

    PluginSystem ps(tmpDir.path());
    auto plugins = ps.discoverPlugins();
    EXPECT_TRUE(plugins.isEmpty());
}

TEST(PluginSystemTest, SkipInvalidToml)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("bad-toml"),
                    QStringLiteral("this is not valid toml [}{"));

    PluginSystem ps(tmpDir.path());
    auto plugins = ps.discoverPlugins();
    // Should skip the invalid plugin
    EXPECT_TRUE(plugins.isEmpty());
}

TEST(PluginSystemTest, SkipManifestWithMissingName)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());

    createPluginDir(tmpDir.path(), QStringLiteral("no-name"),
                    QStringLiteral("[plugin]\nversion = \"1.0.0\"\n"));

    PluginSystem ps(tmpDir.path());
    auto plugins = ps.discoverPlugins();
    EXPECT_TRUE(plugins.isEmpty());
}
