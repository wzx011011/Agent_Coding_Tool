#include <QSignalSpy>
#include <gtest/gtest.h>

#include "framework/stream_formatter.h"
#include "framework/terminal_style.h"

class StreamFormatterTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_prevColor = act::framework::TerminalStyle::colorEnabled();
        act::framework::TerminalStyle::setColorEnabled(false);
    }
    void TearDown() override
    {
        act::framework::TerminalStyle::setColorEnabled(m_prevColor);
    }

    bool m_prevColor = false;
};

// Helper: feed all tokens and collect all emitted lines
static QStringList feedAndCollect(const QStringList &tokens)
{
    act::framework::StreamFormatter formatter;
    QStringList lines;
    QObject::connect(formatter,
                     &act::framework::StreamFormatter::formattedLineReady,
                     [&](const QString &line) { lines.append(line); });
    for (const auto &t : tokens)
        formatter->feedToken(t);
    formatter->flush();
    return lines;
}

TEST_F(StreamFormatterTest, SingleLineToken)
{
    act::framework::StreamFormatter fmt;
    QStringList lines;
    QObject::connect(&fmt, &act::framework::StreamFormatter::formattedLineReady,
                     [&](const QString &line) { lines.append(line); });
    fmt.feedToken(QStringLiteral("Hello world\n"));
    EXPECT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], QStringLiteral("Hello world\n"));
}

TEST_F(StreamFormatterTest, MultiLineToken)
{
    act::framework::StreamFormatter fmt;
    QStringList lines;
    QObject::connect(&fmt, &act::framework::StreamFormatter::formattedLineReady,
                     [&](const QString &line) { lines.append(line); });
    fmt.feedToken(QStringLiteral("line1\nline2\nline3\n"));
    EXPECT_EQ(lines.size(), 3);
    EXPECT_EQ(lines[0], QStringLiteral("line1\n"));
    EXPECT_EQ(lines[1], QStringLiteral("line2\n"));
    EXPECT_EQ(lines[2], QStringLiteral("line3\n"));
}

TEST_F(StreamFormatterTest, IncompleteLineFlushed)
{
    QStringList lines = feedAndCollect({QStringLiteral("partial")});
    EXPECT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], QStringLiteral("partial\n"));
}

TEST_F(StreamFormatterTest, EmptyTokenIgnored)
{
    act::framework::StreamFormatter fmt;
    int callCount = 0;
    QObject::connect(&fmt, &act::framework::StreamFormatter::formattedLineReady,
                     [&](const QString &) { ++callCount; });
    fmt.feedToken(QString());
    EXPECT_EQ(callCount, 0);
}

TEST_F(StreamFormatterTest, CodeBlockCollectedAndFormatted)
{
    QStringList lines = feedAndCollect({
        QStringLiteral("Before code.\n"),
        QStringLiteral("```cpp\n"),
        QStringLiteral("int x = 42;\n"),
        QStringLiteral("```\n"),
        QStringLiteral("After code.\n"),
    });
    // Should have: "Before code.\n", formatted code block, "After code.\n"
    EXPECT_TRUE(lines.size() >= 3);
    EXPECT_TRUE(lines[0].contains(QStringLiteral("Before code")));
    EXPECT_TRUE(lines.last().contains(QStringLiteral("After code")));
    // Code block should have box-drawing characters
    bool hasBox = false;
    for (const auto &l : lines)
    {
        if (l.contains(QString::fromUtf8("\xe2\x94\x8c")) || // ┌
            l.contains(QString::fromUtf8("\xe2\x94\x90")) || // ┐
            l.contains(QString::fromUtf8("\xe2\x94\x94")) || // └
            l.contains(QString::fromUtf8("\xe2\x94\x98")))   // ┘
        {
            hasBox = true;
            break;
        }
    }
    EXPECT_TRUE(hasBox);
}

TEST_F(StreamFormatterTest, UnclosedCodeBlockFlushed)
{
    QStringList lines = feedAndCollect({
        QStringLiteral("```python\n"),
        QStringLiteral("print('hello')\n"),
        // No closing fence
    });
    // Should still emit the code block via flush
    bool hasBox = false;
    for (const auto &l : lines)
    {
        if (l.contains(QString::fromUtf8("\xe2\x94\x94")) ||
            l.contains(QStringLiteral("└")))
        {
            hasBox = true;
            break;
        }
    }
    EXPECT_TRUE(hasBox);
}

TEST_F(StreamFormatterTest, BoldFormattedInStream)
{
    QStringList lines = feedAndCollect({
        QStringLiteral("This is **bold** text.\n"),
    });
    ASSERT_EQ(lines.size(), 1);
    EXPECT_TRUE(lines[0].contains(QStringLiteral("BOLD")));
}

TEST_F(StreamFormatterTest, HeadingFormattedInStream)
{
    QStringList lines = feedAndCollect({
        QStringLiteral("# My Heading\n"),
    });
    ASSERT_EQ(lines.size(), 1);
    EXPECT_TRUE(lines[0].contains(QStringLiteral("My Heading")));
}

TEST_F(StreamFormatterTest, ListFormattedInStream)
{
    QStringList lines = feedAndCollect({
        QStringLiteral("- First item\n"),
        QStringLiteral("- Second item\n"),
    });
    ASSERT_EQ(lines.size(), 2);
    EXPECT_TRUE(lines[0].contains(QStringLiteral("\u2022 First item")));
    EXPECT_TRUE(lines[1].contains(QStringLiteral("\u2022 Second item")));
}

TEST_F(StreamFormatterTest, HorizontalRuleFormattedInStream)
{
    QStringList lines = feedAndCollect({
        QStringLiteral("---\n"),
    });
    ASSERT_EQ(lines.size(), 1);
    EXPECT_TRUE(lines[0].contains(QString::fromUtf8("\u2500")));
}

TEST_F(StreamFormatterTest, EmptyLinesPreserved)
{
    QStringList lines = feedAndCollect({
        QStringLiteral("\n"),
    });
    EXPECT_EQ(lines.size(), 1);
}
