#include "SpirvTranslator.hpp"

#include <algorithm>

#include "TargetBindings.hpp"

namespace haylen::shaderc {

Translation SpirvTranslator::translate(const Package::Program& program, const StageCompiler::Result& compiled) {
    Translation translation{.code = {.target = Package::Target::Spirv}};
    TargetBindings::assignSets(program, translation.code);
    translation.code.vertex.entry = "main";
    translation.code.fragment.entry = "main";
    translation.vertexSource = encode(TargetBindings::rebind(compiled.vertex, translation.code));
    translation.fragmentSource = encode(TargetBindings::rebind(compiled.fragment, translation.code));
    return translation;
}

std::string SpirvTranslator::encode(const std::vector<std::uint32_t>& words) {
    // The bytes of every word go out from the lowest to the highest, whatever the order of the machine, and every three bytes become four characters.
    std::vector<std::uint8_t> bytes;
    bytes.reserve(words.size() * 4);
    for (const std::uint32_t word : words) {
        for (int shift = 0; shift < 32; shift += 8) {
            bytes.push_back(static_cast<std::uint8_t>(word >> shift));
        }
    }

    std::string text;
    text.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t index = 0; index < bytes.size(); index += 3) {
        const std::size_t count = std::min<std::size_t>(3, bytes.size() - index);
        std::uint32_t group = static_cast<std::uint32_t>(bytes[index]) << 16;
        group |= count > 1 ? static_cast<std::uint32_t>(bytes[index + 1]) << 8 : 0;
        group |= count > 2 ? static_cast<std::uint32_t>(bytes[index + 2]) : 0;
        for (std::size_t character = 0; character < 4; ++character) {
            text += character <= count ? kAlphabet[(group >> (18 - 6 * character)) & 0x3F] : '=';
        }
    }
    return text;
}

} // namespace haylen::shaderc
