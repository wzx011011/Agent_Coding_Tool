#include <gtest/gtest.h>

#include <QJsonObject>

#include "framework/skill_catalog.h"

using namespace act::framework;

// ============================================================
// SkillCatalog Tests
// ============================================================

static SkillEntry makeSimpleSkill(const QString &name)
{
    SkillEntry s;
    s.name = name;
    return s;
}

TEST(SkillCatalogTest, RegisterAndFind)
{
    SkillCatalog catalog;
    SkillEntry skill;
    skill.name = QStringLiteral("commit");
    skill.description = QStringLiteral("Create a git commit");
    skill.systemPrompt = QStringLiteral("You are a commit assistant.");
    skill.body = QStringLiteral("Follow conventional commits.");

    EXPECT_TRUE(catalog.registerSkill(skill));
    EXPECT_EQ(catalog.size(), 1);
    EXPECT_NE(catalog.findSkill(QStringLiteral("commit")), nullptr);
    EXPECT_EQ(catalog.findSkill(QStringLiteral("commit"))->name, QStringLiteral("commit"));
}

TEST(SkillCatalogTest, DuplicateRegistrationFails)
{
    SkillCatalog catalog;
    SkillEntry skill;
    skill.name = QStringLiteral("commit");
    skill.description = QStringLiteral("desc");

    EXPECT_TRUE(catalog.registerSkill(skill));
    EXPECT_FALSE(catalog.registerSkill(skill));
    EXPECT_EQ(catalog.size(), 1);
}

TEST(SkillCatalogTest, Unregister)
{
    SkillCatalog catalog;
    SkillEntry skill;
    skill.name = QStringLiteral("commit");

    catalog.registerSkill(skill);
    EXPECT_TRUE(catalog.unregisterSkill(QStringLiteral("commit")));
    EXPECT_EQ(catalog.size(), 0);
    EXPECT_EQ(catalog.findSkill(QStringLiteral("commit")), nullptr);
}

TEST(SkillCatalogTest, UnregisterNonexistent)
{
    SkillCatalog catalog;
    EXPECT_FALSE(catalog.unregisterSkill(QStringLiteral("ghost")));
}

TEST(SkillCatalogTest, FindNonexistent)
{
    SkillCatalog catalog;
    EXPECT_EQ(catalog.findSkill(QStringLiteral("ghost")), nullptr);
}

TEST(SkillCatalogTest, ListSkills)
{
    SkillCatalog catalog;
    catalog.registerSkill(makeSimpleSkill(QStringLiteral("a")));
    catalog.registerSkill(makeSimpleSkill(QStringLiteral("b")));
    catalog.registerSkill(makeSimpleSkill(QStringLiteral("c")));

    auto names = catalog.listSkills();
    EXPECT_EQ(names.size(), 3);
    EXPECT_TRUE(names.contains(QStringLiteral("a")));
    EXPECT_TRUE(names.contains(QStringLiteral("b")));
    EXPECT_TRUE(names.contains(QStringLiteral("c")));
}

TEST(SkillCatalogTest, FindByTriggers)
{
    SkillCatalog catalog;

    SkillEntry commit;
    commit.name = QStringLiteral("commit");
    commit.triggers = {QStringLiteral("commit"), QStringLiteral("git")};

    SkillEntry review;
    review.name = QStringLiteral("review-pr");
    review.triggers = {QStringLiteral("review"), QStringLiteral("pr")};

    catalog.registerSkill(commit);
    catalog.registerSkill(review);

    auto results = catalog.findByTriggers({QStringLiteral("git")});
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results.first()->name, QStringLiteral("commit"));

    results = catalog.findByTriggers({QStringLiteral("pr"), QStringLiteral("review")});
    ASSERT_EQ(results.size(), 2);
}

TEST(SkillCatalogTest, FindByTriggersCaseInsensitive)
{
    SkillCatalog catalog;
    SkillEntry skill;
    skill.name = QStringLiteral("commit");
    skill.triggers = {QStringLiteral("COMMIT")};

    catalog.registerSkill(skill);
    auto results = catalog.findByTriggers({QStringLiteral("commit")});
    EXPECT_EQ(results.size(), 1);
}

TEST(SkillCatalogTest, BuildSystemPrompt)
{
    SkillCatalog catalog;

    SkillEntry s1;
    s1.name = QStringLiteral("commit");
    s1.systemPrompt = QStringLiteral("Be concise.");

    SkillEntry s2;
    s2.name = QStringLiteral("review");
    s2.systemPrompt = QStringLiteral("Be thorough.");
    s2.body = QStringLiteral("Body content.");  // body should NOT appear

    catalog.registerSkill(s1);
    catalog.registerSkill(s2);

    auto prompt = catalog.buildSystemPrompt();
    EXPECT_TRUE(prompt.contains(QStringLiteral("Skill: commit")));
    EXPECT_TRUE(prompt.contains(QStringLiteral("Be concise.")));
    EXPECT_TRUE(prompt.contains(QStringLiteral("Skill: review")));
    EXPECT_TRUE(prompt.contains(QStringLiteral("Be thorough.")));
    EXPECT_FALSE(prompt.contains(QStringLiteral("Body content.")));
}

TEST(SkillCatalogTest, BuildSystemPromptEmptyWhenNoPrompts)
{
    SkillCatalog catalog;
    SkillEntry skill;
    skill.name = QStringLiteral("test");
    skill.body = QStringLiteral("Has body but no prompt.");

    catalog.registerSkill(skill);
    auto prompt = catalog.buildSystemPrompt();
    EXPECT_TRUE(prompt.isEmpty());
}

TEST(SkillCatalogTest, SkillBody)
{
    SkillCatalog catalog;
    SkillEntry skill;
    skill.name = QStringLiteral("commit");
    skill.body = QStringLiteral("Follow conventional commits.");

    catalog.registerSkill(skill);
    EXPECT_EQ(catalog.skillBody(QStringLiteral("commit")),
              QStringLiteral("Follow conventional commits."));
    EXPECT_TRUE(catalog.skillBody(QStringLiteral("nonexistent")).isEmpty());
}

TEST(SkillCatalogTest, FullSkillWithMetadata)
{
    SkillCatalog catalog;
    SkillEntry skill;
    skill.name = QStringLiteral("commit");
    skill.version = QStringLiteral("1.0.0");
    skill.description = QStringLiteral("Commit helper");
    skill.systemPrompt = QStringLiteral("Be concise.");
    skill.body = QStringLiteral("Use conventional commits.");
    skill.triggers = {QStringLiteral("commit"), QStringLiteral("git")};

    QJsonObject meta;
    meta[QStringLiteral("author")] = QStringLiteral("ACT");
    skill.metadata = meta;

    EXPECT_TRUE(catalog.registerSkill(skill));
    auto *found = catalog.findSkill(QStringLiteral("commit"));
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->version, QStringLiteral("1.0.0"));
    EXPECT_EQ(found->metadata.value(QStringLiteral("author")),
              QStringLiteral("ACT"));
    EXPECT_EQ(found->triggers.size(), 2);
}

// ============================================================
// SkillTrace Tests
// ============================================================

TEST(SkillTraceTest, CatalogTracksLoadCount)
{
    SkillCatalog catalog;
    catalog.registerSkill(makeSimpleSkill(QStringLiteral("a")));
    catalog.registerSkill(makeSimpleSkill(QStringLiteral("b")));
    catalog.registerSkill(makeSimpleSkill(QStringLiteral("c")));

    EXPECT_EQ(catalog.size(), 3);
}
