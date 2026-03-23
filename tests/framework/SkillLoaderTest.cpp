#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "framework/skill_loader.h"

using namespace act::framework;

class SkillLoaderTest : public ::testing::Test
{
protected:
    void SetUp() override { tmpDir.emplace(); }
    void TearDown() override { tmpDir.reset(); }
    std::optional<QTemporaryDir> tmpDir;
};

TEST_F(SkillLoaderTest, LoadFromNonexistentDir)
{
    SkillCatalog catalog;
    SkillLoader loader;
    EXPECT_EQ(loader.loadFromDirectory(QStringLiteral("/nonexistent/path"), catalog), 0);
    EXPECT_FALSE(loader.errors().isEmpty());
}

TEST_F(SkillLoaderTest, LoadFromEmptyDir)
{
    SkillCatalog catalog;
    SkillLoader loader;
    EXPECT_EQ(loader.loadFromDirectory(tmpDir->path(), catalog), 0);
    EXPECT_TRUE(loader.errors().isEmpty());
}

TEST_F(SkillLoaderTest, LoadSingleSkillFile)
{
    QString filePath = QDir::cleanPath(
        tmpDir->path() + QLatin1Char('/') + QStringLiteral("commit.toml"));
    QFile file(filePath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QByteArray tomlData =
        "name = \"commit\"\n"
        "version = \"1.0.0\"\n"
        "description = \"Git commit helper\"\n"
        "system_prompt = \"Be concise.\"\n"
        "body = \"Follow conventional commits.\"\n"
        "triggers = [\"commit\", \"git\"]\n";
    file.write(tomlData);
    file.close();

    SkillCatalog catalog;
    SkillLoader loader;
    int loaded = loader.loadFromDirectory(tmpDir->path(), catalog);
    EXPECT_EQ(loaded, 1);
    EXPECT_TRUE(loader.errors().isEmpty());
    auto *s = catalog.findSkill(QStringLiteral("commit"));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->description, QStringLiteral("Git commit helper"));
    EXPECT_EQ(s->body, QStringLiteral("Follow conventional commits."));
    EXPECT_EQ(s->triggers.size(), 2);
}

TEST_F(SkillLoaderTest, InvalidTomlReportsError)
{
    QFile file(tmpDir->path() + QLatin1String("/bad.toml"));
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write("this is not valid toml!!!");
    file.close();

    SkillCatalog catalog;
    SkillLoader loader;
    int loaded = loader.loadFromDirectory(tmpDir->path(), catalog);
    EXPECT_EQ(loaded, 0);
    EXPECT_FALSE(loader.errors().isEmpty());
}

TEST_F(SkillLoaderTest, MissingNameReportsError)
{
    QFile file(tmpDir->path() + QLatin1String("/nameless.toml"));
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write("description = \"No name field\"");
    file.close();

    SkillCatalog catalog;
    SkillLoader loader;
    int loaded = loader.loadFromDirectory(tmpDir->path(), catalog);
    EXPECT_EQ(loaded, 0);
    EXPECT_FALSE(loader.errors().isEmpty());
}

TEST_F(SkillLoaderTest, DuplicateSkillAcrossFiles)
{
    for (const auto &name : {QStringLiteral("dup.toml"), QStringLiteral("dup2.toml")})
    {
        QFile file(tmpDir->path() + QLatin1Char('/') + name);
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        file.write("name = \"dup\"");
        file.close();
    }

    SkillCatalog catalog;
    SkillLoader loader;
    int loaded = loader.loadFromDirectory(tmpDir->path(), catalog);
    EXPECT_EQ(loaded, 1);
    EXPECT_FALSE(loader.errors().isEmpty());
}

TEST_F(SkillLoaderTest, LoadSkillWithMetadata)
{
    QFile file(tmpDir->path() + QLatin1String("/meta.toml"));
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QByteArray tomlData =
        "name = \"meta\"\n"
        "version = \"2.0\"\n"
        "[metadata]\n"
        "author = \"test\"\n"
        "tags = [\"auto\", \"ci\"]\n";
    file.write(tomlData);
    file.close();

    SkillCatalog catalog;
    SkillLoader loader;
    loader.loadFromDirectory(tmpDir->path(), catalog);

    auto *skill = catalog.findSkill(QStringLiteral("meta"));
    ASSERT_NE(skill, nullptr);
    EXPECT_EQ(skill->metadata.value(QStringLiteral("author")),
              QStringLiteral("test"));
}
