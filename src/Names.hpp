#pragma once

#include <span>
#include <string>
#include <string_view>

namespace haylen::shaderc {

// The names that sources and arguments give, and how messages list them.
class Names {
  public:
    // Tells whether a name starts with a letter or an underscore and goes on with letters, digits and underscores.
    [[nodiscard]] static bool isIdentifier(std::string_view value) noexcept;

    // Lists names in double quotes, joined by commas and the conjunction before the last one, such as "a", "b" or "c".
    [[nodiscard]] static std::string quoted(std::span<const std::string_view> names, std::string_view conjunction);
};

} // namespace haylen::shaderc
