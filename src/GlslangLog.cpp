#include "GlslangLog.hpp"

#include <cctype>
#include <format>

namespace haylen::shaderc {

std::vector<Diagnostic> GlslangLog::parse(std::string_view log, const ShaderSource::Stage& stage, const ShaderSource& source) {
    std::vector<Diagnostic> diagnostics;
    std::size_t start = 0;
    while (start < log.size()) {
        const std::size_t newline = log.find('\n', start);
        const std::size_t end = newline == std::string_view::npos ? log.size() : newline;
        std::string_view line = trim(log.substr(start, end - start));
        start = end + 1;
        if (!line.starts_with("ERROR: ")) {
            continue;
        }
        line.remove_prefix(7);

        ShaderSource::Location location = stage.location;
        if (const std::optional<ShaderSource::Location> place = readLocation(line)) {
            const bool known = place->file >= 0 && static_cast<std::size_t>(place->file) < source.files.size();
            location = {.file = known ? place->file : stage.location.file, .line = place->line, .column = place->column};
        }

        const std::string_view message = trim(line);
        if (message.find("compilation terminated") != std::string_view::npos || message.find("compilation errors") != std::string_view::npos) {
            continue;
        }
        diagnostics.push_back(source.diagnose(location, toSentence(message)));
    }
    return diagnostics;
}

std::optional<ShaderSource::Location> GlslangLog::readLocation(std::string_view& text) {
    // A place is the number of the source string, which the stage sets to the index of a file, the line and the column, each followed by a colon.
    std::string_view rest = text;
    const std::optional<int> file = readNumber(rest);
    if (!file || !rest.starts_with(':')) {
        return std::nullopt;
    }
    rest.remove_prefix(1);
    const std::optional<int> line = readNumber(rest);
    if (!line || !rest.starts_with(':')) {
        return std::nullopt;
    }
    rest.remove_prefix(1);
    const std::optional<int> column = readNumber(rest);
    if (column && rest.starts_with(':')) {
        rest.remove_prefix(1);
    }
    text = rest;
    return ShaderSource::Location{.file = *file, .line = *line, .column = column.value_or(0)};
}

std::optional<int> GlslangLog::readNumber(std::string_view& text) {
    int value = 0;
    std::size_t digits = 0;
    while (digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits])) != 0) {
        value = value * 10 + (text[digits] - '0');
        ++digits;
    }
    if (digits == 0) {
        return std::nullopt;
    }
    text.remove_prefix(digits);
    return value;
}

std::string GlslangLog::toSentence(std::string_view message) {
    // The messages of glslang read "'token' : description", where the token may be empty.
    std::string_view token;
    std::string_view description = message;
    if (message.starts_with('\'')) {
        const std::size_t close = message.find("' :", 1);
        if (close != std::string_view::npos) {
            token = trim(message.substr(1, close - 1));
            description = trim(message.substr(close + 3));
        }
    }

    // The description quotes types and names in single quotes with spaces inside, which become double quotes around the bare text.
    std::string sentence;
    bool quoted = false;
    std::string fragment;
    for (const char character : description) {
        if (character == '\'') {
            if (quoted) {
                sentence += std::format("\"{}\"", trim(fragment));
                fragment.clear();
            }
            quoted = !quoted;
            continue;
        }
        if (quoted) {
            fragment += character;
        } else if (character != ' ' || (!sentence.empty() && sentence.back() != ' ')) {
            sentence += character;
        }
    }
    if (quoted) {
        sentence += "'" + fragment;
    }

    sentence = std::string(trim(sentence));
    if (!sentence.empty()) {
        sentence.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(sentence.front())));
    }
    if (!token.empty()) {
        sentence += std::format(" (\"{}\")", token);
    }
    if (sentence.empty() || (sentence.back() != '.' && sentence.back() != '?' && sentence.back() != '!')) {
        sentence += '.';
    }
    return sentence;
}

std::string_view GlslangLog::trim(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

} // namespace haylen::shaderc
