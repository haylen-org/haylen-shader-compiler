#include "PathText.hpp"

namespace haylen::shaderc {

std::string PathText::of(const std::filesystem::path& path) {
    const std::u8string text = path.generic_u8string();
    std::string result;
    result.reserve(text.size());
    for (const char8_t unit : text) {
        result.push_back(static_cast<char>(unit));
    }
    return result;
}

std::filesystem::path PathText::from(std::string_view text) {
    std::u8string units;
    units.reserve(text.size());
    for (const char unit : text) {
        units.push_back(static_cast<char8_t>(unit));
    }
    return units;
}

} // namespace haylen::shaderc
