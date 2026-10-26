#include "SourceFile.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

namespace haylen::shaderc {

std::optional<SourceFile> SourceFile::read(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        return std::nullopt;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        return std::nullopt;
    }
    return parse(text);
}

SourceFile SourceFile::parse(std::string_view text) {
    if (text.starts_with("\xEF\xBB\xBF")) {
        text.remove_prefix(3);
    }

    SourceFile file;
    bool inComment = false;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t newline = text.find('\n', start);
        const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
        std::string_view content = text.substr(start, end - start);
        if (content.ends_with('\r')) {
            content.remove_suffix(1);
        }

        Line line{.number = static_cast<int>(file.lines.size()) + 1, .text = std::string(content)};
        scan(line, inComment);
        file.lines.push_back(std::move(line));
        start = end + 1;
    }
    return file;
}

void SourceFile::scan(Line& line, bool& inComment) {
    const std::string_view text = line.text;
    std::size_t position = 0;
    std::size_t tagStart = std::string_view::npos;
    std::size_t tagEnd = text.size();
    while (position < text.size()) {
        if (inComment) {
            const std::size_t close = text.find("*/", position);
            if (close == std::string_view::npos) {
                break;
            }
            inComment = false;
            position = close + 2;
            continue;
        }

        const char character = text[position];
        if (character == ' ' || character == '\t') {
            ++position;
            continue;
        }
        if (text.substr(position, 2) == "//" || text.substr(position, 2) == "/*") {
            if (tagStart != std::string_view::npos && tagEnd == text.size()) {
                tagEnd = position;
            }
            if (text.substr(position, 2) == "//") {
                break;
            }
            inComment = true;
            position += 2;
            continue;
        }

        if (line.codeColumn == 0) {
            line.codeColumn = static_cast<int>(position) + 1;
            if (character == '@') {
                tagStart = position;
            }
        }
        ++position;
    }

    if (tagStart != std::string_view::npos) {
        line.tag = split(text, tagStart, tagEnd);
    }
}

std::vector<SourceFile::Word> SourceFile::split(std::string_view text, std::size_t start, std::size_t end) {
    std::vector<Word> words;
    std::size_t position = start;
    while (position < end) {
        if (text[position] == ' ' || text[position] == '\t') {
            ++position;
            continue;
        }
        const std::size_t first = position;
        while (position < end && text[position] != ' ' && text[position] != '\t') {
            ++position;
        }
        words.push_back({.text = std::string(text.substr(first, position - first)), .column = static_cast<int>(first) + 1});
    }
    return words;
}

} // namespace haylen::shaderc
