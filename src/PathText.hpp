#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace haylen::shaderc {

// The UTF-8 text of paths in arguments, sources, messages and packages.
class PathText {
  public:
    // Returns the path in UTF-8 with forward slashes on every system.
    [[nodiscard]] static std::string of(const std::filesystem::path& path);

    [[nodiscard]] static std::filesystem::path from(std::string_view text);
};

} // namespace haylen::shaderc
