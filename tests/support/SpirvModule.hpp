#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::shaderc::test {

// Reads the SPIR-V modules of the "spirv" target from the base64 text of the package, as a Vulkan device does.
class SpirvModule {
  public:
    // Returns the words of a module, or nothing for text that is not base64 of whole words in little-endian order.
    [[nodiscard]] static std::optional<std::vector<std::uint32_t>> decode(std::string_view text);

    // Returns the module as SPIR-V assembly with the names of its objects, or nothing for text that is not a module.
    [[nodiscard]] static std::optional<std::string> disassemble(std::string_view text);

  private:
    [[nodiscard]] static int valueOf(char character) noexcept;
};

} // namespace haylen::shaderc::test
