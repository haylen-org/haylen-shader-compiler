#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::shaderc {

// The lines of a source file, each one either a tag with its arguments or a line of code. Comments never hold tags.
class SourceFile {
  public:
    struct Word {
        std::string text;
        int column = 0;
    };

    struct Line {
        int number = 0;
        std::string text;
        // The tag that starts the line and the arguments that follow it, empty for a line of code.
        std::vector<Word> tag;
        // The column of the first character outside comments, 0 for a line of white space and comments.
        int codeColumn = 0;
    };

    std::vector<Line> lines;

    [[nodiscard]] static std::optional<SourceFile> read(const std::filesystem::path& path);
    [[nodiscard]] static SourceFile parse(std::string_view text);

  private:
    static void scan(Line& line, bool& inComment);
    [[nodiscard]] static std::vector<Word> split(std::string_view text, std::size_t start, std::size_t end);
};

} // namespace haylen::shaderc
