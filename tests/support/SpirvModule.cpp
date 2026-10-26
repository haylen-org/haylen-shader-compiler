#include "support/SpirvModule.hpp"

#include <spirv-tools/libspirv.hpp>

namespace haylen::shaderc::test {

std::optional<std::vector<std::uint32_t>> SpirvModule::decode(std::string_view text) {
    if (text.size() % 4 != 0) {
        return std::nullopt;
    }
    const std::size_t padding = text.ends_with("==") ? 2 : text.ends_with('=') ? 1 : 0;

    std::vector<std::uint8_t> bytes;
    for (std::size_t index = 0; index < text.size(); index += 4) {
        std::uint32_t group = 0;
        for (std::size_t character = 0; character < 4; ++character) {
            const bool padded = index + character >= text.size() - padding;
            const int value = padded ? 0 : valueOf(text[index + character]);
            if (value < 0) {
                return std::nullopt;
            }
            group = (group << 6) | static_cast<std::uint32_t>(value);
        }
        for (int shift = 16; shift >= 0; shift -= 8) {
            bytes.push_back(static_cast<std::uint8_t>(group >> shift));
        }
    }
    bytes.resize(bytes.size() - padding);
    if (bytes.size() % 4 != 0) {
        return std::nullopt;
    }

    std::vector<std::uint32_t> words(bytes.size() / 4);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        words[index / 4] |= static_cast<std::uint32_t>(bytes[index]) << (8 * (index % 4));
    }
    return words;
}

std::optional<std::string> SpirvModule::disassemble(std::string_view text) {
    const std::optional<std::vector<std::uint32_t>> words = decode(text);
    std::string assembly;
    const spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_1);
    if (!words || !tools.Disassemble(*words, &assembly, SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES)) {
        return std::nullopt;
    }
    return assembly;
}

int SpirvModule::valueOf(char character) noexcept {
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    }
    if (character >= 'a' && character <= 'z') {
        return character - 'a' + 26;
    }
    if (character >= '0' && character <= '9') {
        return character - '0' + 52;
    }
    if (character == '+') {
        return 62;
    }
    return character == '/' ? 63 : -1;
}

} // namespace haylen::shaderc::test
