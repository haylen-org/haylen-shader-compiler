#include "Names.hpp"

#include <format>

namespace haylen::shaderc {

bool Names::isIdentifier(std::string_view value) noexcept {
    if (value.empty() || (value.front() >= '0' && value.front() <= '9')) {
        return false;
    }
    for (const char character : value) {
        const bool letter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
        if (!letter && !(character >= '0' && character <= '9') && character != '_') {
            return false;
        }
    }
    return true;
}

std::string Names::quoted(std::span<const std::string_view> names, std::string_view conjunction) {
    std::string text;
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index > 0) {
            text += index + 1 == names.size() ? std::format(" {} ", conjunction) : std::string(", ");
        }
        text += std::format("\"{}\"", names[index]);
    }
    return text;
}

} // namespace haylen::shaderc
