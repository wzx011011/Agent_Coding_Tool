#include <gtest/gtest.h>

#include "framework/markdown_formatter.h"
#include "framework/terminal_style.h"

static QString esc(int n) { return QString::fromUtf8("\x1b[" + std::to_string(n) + "m"); }

TEST(MarkdownFormatterTest, PlainTextUnchanged)
{
    QString input = QStringLiteral("Hello world");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("Hello world")));
}

TEST(MarkdownFormatterTest, CodeBlockFormatting)
{
    QString input =
        QStringLiteral("```cpp\nint x = 42;\n```\n");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("int x = 42;")));
    EXPECT_TRUE(result.contains(QStringLiteral("┌")));
    EXPECT_TRUE(result.contains(QStringLiteral("└")));
}

TEST(MarkdownFormatterTest, CodeBlockWithLanguage)
{
    QString input =
        QStringLiteral("```python\nprint('hello')\n```\n");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("python")));
    EXPECT_TRUE(result.contains(QStringLiteral("print('hello')")));
}

TEST(MarkdownFormatterTest, InlineCodeFormatting)
{
    QString input = QStringLiteral("Use `std::format` for output.");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("[std::format]")));
}

TEST(MarkdownFormatterTest, HeadingLevel1)
{
    QString input = QStringLiteral("# Title\n");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("Title")));
    EXPECT_TRUE(result.contains(QStringLiteral("═")));
}

TEST(MarkdownFormatterTest, HeadingLevel2)
{
    QString input = QStringLiteral("## Section\n");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("Section")));
    EXPECT_TRUE(result.contains(QStringLiteral("═")));
}

TEST(MarkdownFormatterTest, HeadingLevel3)
{
    QString input = QStringLiteral("### Subsection\n");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("Subsection")));
    EXPECT_TRUE(result.contains(QStringLiteral("─")));
}

TEST(MarkdownFormatterTest, BoldFormatting)
{
    QString input = QStringLiteral("This is **important** text.");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("IMPORTANT")));
}

TEST(MarkdownFormatterTest, UnorderedListFormatting)
{
    QString input =
        QStringLiteral("- First item\n- Second item\n- Third item\n");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("•")));
    EXPECT_TRUE(result.contains(QStringLiteral("First item")));
    EXPECT_TRUE(result.contains(QStringLiteral("Second item")));
}

TEST(MarkdownFormatterTest, HorizontalRuleFormatting)
{
    QString input = QStringLiteral("---\n");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("─")));
}

TEST(MarkdownFormatterTest, CombinedFormatting)
{
    QString input =
        QStringLiteral("# Test\n\nHere is some **bold** text "
                       "with `code` and a list:\n- item 1\n- item 2\n");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("Test")));
    EXPECT_TRUE(result.contains(QStringLiteral("BOLD")));
    EXPECT_TRUE(result.contains(QStringLiteral("[code]")));
    EXPECT_TRUE(result.contains(QStringLiteral("•")));
}

TEST(MarkdownFormatterTest, EmptyString)
{
    QString result = act::framework::MarkdownFormatter::format(QString());
    EXPECT_TRUE(result.isEmpty());
}

TEST(MarkdownFormatterTest, CodeBlocksAreVisuallySeparated)
{
    // Code blocks are visually separated with borders
    QString input =
        QStringLiteral("```\nint x = 42;\n```\n");
    QString result = act::framework::MarkdownFormatter::format(input);
    EXPECT_TRUE(result.contains(QStringLiteral("int x = 42;")));
    EXPECT_TRUE(result.contains(QStringLiteral("┌")));
    EXPECT_TRUE(result.contains(QStringLiteral("└")));
}

// ============================================================
// Color-enabled tests
// ============================================================

class MarkdownColorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_prevColor = act::framework::TerminalStyle::colorEnabled();
        act::framework::TerminalStyle::setColorEnabled(true);
    }
    void TearDown() override
    {
        act::framework::TerminalStyle::setColorEnabled(m_prevColor);
    }

    bool m_prevColor = false;
};

TEST_F(MarkdownColorTest, BoldWithColorUsesAnsiBold)
{
    QString input = QStringLiteral("This is **important** text.");
    QString result = act::framework::MarkdownFormatter::format(input, true);
    EXPECT_TRUE(result.contains(QStringLiteral("important")));
    // Should NOT be uppercase when color is on
    EXPECT_FALSE(result.contains(QStringLiteral("IMPORTANT")));
    EXPECT_TRUE(result.contains(esc(1))); // bold
}

TEST_F(MarkdownColorTest, InlineCodeWithColor)
{
    QString input = QStringLiteral("Use `std::format` here.");
    QString result = act::framework::MarkdownFormatter::format(input, true);
    // ANSI codes are embedded between [ and ] brackets, so strip to check structure
    QString stripped = act::framework::TerminalStyle::stripAnsi(result);
    EXPECT_TRUE(stripped.contains(QStringLiteral("[std::format]")));
    EXPECT_TRUE(result.contains(esc(36))); // cyan
}

TEST_F(MarkdownColorTest, HeadingWithColor)
{
    QString input = QStringLiteral("# Title\n");
    QString result = act::framework::MarkdownFormatter::format(input, true);
    EXPECT_TRUE(result.contains(QStringLiteral("Title")));
    EXPECT_TRUE(result.contains(esc(1))); // bold
}

TEST_F(MarkdownColorTest, CodeBlockLangWithColor)
{
    QString input = QStringLiteral("```cpp\nint x = 1;\n```\n");
    QString result = act::framework::MarkdownFormatter::format(input, true);
    EXPECT_TRUE(result.contains(esc(36))); // cyan for lang label
}

TEST_F(MarkdownColorTest, HorizontalRuleWithColor)
{
    QString input = QStringLiteral("---\n");
    QString result = act::framework::MarkdownFormatter::format(input, true);
    EXPECT_TRUE(result.contains(esc(0))); // dim wraps with reset
}
