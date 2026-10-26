#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SourceFile.hpp"

namespace haylen::shaderc {

TEST(SourceFileTest, SplitsTagsIntoWordsWithTheirColumns) {
    const SourceFile file = SourceFile::parse("  @program sprite\tsprite_vs  sprite_fs // The sprite program.\n");

    ASSERT_EQ(file.lines.size(), 1U);
    const std::vector<SourceFile::Word>& tag = file.lines[0].tag;
    ASSERT_EQ(tag.size(), 4U);
    EXPECT_EQ(tag[0].text, "@program");
    EXPECT_EQ(tag[0].column, 3);
    EXPECT_EQ(tag[2].text, "sprite_vs");
    EXPECT_EQ(tag[2].column, 19);
    EXPECT_EQ(tag[3].text, "sprite_fs");
    EXPECT_EQ(file.lines[0].codeColumn, 3);
}

TEST(SourceFileTest, ReadsTagsOnlyOutsideComments) {
    const SourceFile file = SourceFile::parse("/* Starts a comment.\n@vs hidden\n*/ @fs late\n// @end\nfloat value; /* Opens another.\n@end */\n");

    ASSERT_EQ(file.lines.size(), 6U);
    EXPECT_TRUE(file.lines[0].tag.empty());
    EXPECT_EQ(file.lines[0].codeColumn, 0);
    EXPECT_TRUE(file.lines[1].tag.empty());
    EXPECT_EQ(file.lines[1].codeColumn, 0);
    ASSERT_EQ(file.lines[2].tag.size(), 2U);
    EXPECT_EQ(file.lines[2].tag[0].column, 4);
    EXPECT_TRUE(file.lines[3].tag.empty());
    EXPECT_EQ(file.lines[4].codeColumn, 1);
    EXPECT_TRUE(file.lines[5].tag.empty());
}

TEST(SourceFileTest, HandlesWindowsLineEndingsAndByteOrderMarks) {
    const SourceFile file = SourceFile::parse("\xEF\xBB\xBF@vs main_vs\r\nvoid main() {}\r\n@end");

    ASSERT_EQ(file.lines.size(), 3U);
    EXPECT_EQ(file.lines[0].tag[1].text, "main_vs");
    EXPECT_EQ(file.lines[0].tag[0].column, 1);
    EXPECT_EQ(file.lines[1].text, "void main() {}");
    EXPECT_EQ(file.lines[2].tag[0].text, "@end");
    EXPECT_EQ(file.lines[2].number, 3);
}

} // namespace haylen::shaderc
