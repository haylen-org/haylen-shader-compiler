#include "SpirvPatcher.hpp"

#include <array>

#include <GLSL.std.450.h>
#include <spirv.hpp>

namespace haylen::shaderc {

std::vector<std::uint32_t> SpirvPatcher::rebind(std::vector<std::uint32_t> words, const std::map<std::uint32_t, Binding>& bindings) {
    // Every instruction starts with its word count in the high half of its first word.
    std::size_t position = kHeaderWords;
    while (position < words.size()) {
        const std::uint32_t count = words[position] >> 16;
        const std::uint32_t opcode = words[position] & 0xFFFF;
        if (opcode == spv::OpDecorate && count >= 4) {
            const auto found = bindings.find(words[position + 1]);
            const std::uint32_t decoration = words[position + 2];
            if (found != bindings.end() && decoration == spv::DecorationDescriptorSet) {
                words[position + 3] = found->second.set;
            } else if (found != bindings.end() && decoration == spv::DecorationBinding) {
                words[position + 3] = found->second.binding;
            }
        }
        position += count;
    }
    return words;
}

std::vector<std::uint32_t> SpirvPatcher::refineDerivatives(std::vector<std::uint32_t> words) {
    bool capable = false;
    bool refined = false;
    for (std::size_t position = kHeaderWords; position < words.size(); position += words[position] >> 16) {
        const std::uint32_t count = words[position] >> 16;
        const std::uint32_t opcode = words[position] & 0xFFFF;
        capable = capable || (opcode == spv::OpCapability && words[position + 1] == spv::CapabilityDerivativeControl);
        if (opcode == spv::OpDPdx || opcode == spv::OpDPdy || opcode == spv::OpFwidth) {
            const std::uint32_t fine = opcode == spv::OpDPdx ? spv::OpDPdxFine : opcode == spv::OpDPdy ? spv::OpDPdyFine : spv::OpFwidthFine;
            words[position] = count << 16 | fine;
            refined = true;
        }
    }

    // The fine instructions need their capability, which goes first since capabilities open a module.
    if (refined && !capable) {
        const std::array<std::uint32_t, 2> capability = {2U << 16 | spv::OpCapability, spv::CapabilityDerivativeControl};
        words.insert(words.begin() + static_cast<std::ptrdiff_t>(kHeaderWords), capability.begin(), capability.end());
    }
    return words;
}

std::vector<std::uint32_t> SpirvPatcher::expandFineWidths(const std::vector<std::uint32_t>& words) {
    // The import of the instructions of GLSL, which glslang writes into every module, gives the absolute values.
    std::uint32_t glsl = 0;
    bool widths = false;
    for (std::size_t position = kHeaderWords; position < words.size(); position += words[position] >> 16) {
        const std::uint32_t count = words[position] >> 16;
        const std::uint32_t opcode = words[position] & 0xFFFF;
        if (opcode == spv::OpExtInstImport && readLiteral(words, position + 2, count - 2) == "GLSL.std.450") {
            glsl = words[position + 1];
        }
        widths = widths || opcode == spv::OpFwidthFine;
    }
    if (!widths) {
        return words;
    }

    // Every fine fwidth becomes the sum of the absolute fine derivatives, in ids past the bound of the module.
    std::vector<std::uint32_t> expanded(words.begin(), words.begin() + static_cast<std::ptrdiff_t>(kHeaderWords));
    expanded.reserve(words.size() + words.size() / 4);
    std::uint32_t bound = words[kBoundWord];
    for (std::size_t position = kHeaderWords; position < words.size(); position += words[position] >> 16) {
        const std::uint32_t count = words[position] >> 16;
        if ((words[position] & 0xFFFF) == spv::OpFwidthFine) {
            const std::uint32_t type = words[position + 1];
            const std::uint32_t value = words[position + 3];
            const std::uint32_t across = bound++;
            const std::uint32_t down = bound++;
            const std::uint32_t acrossSize = bound++;
            const std::uint32_t downSize = bound++;
            append(expanded, spv::OpDPdxFine, {type, across, value});
            append(expanded, spv::OpDPdyFine, {type, down, value});
            append(expanded, spv::OpExtInst, {type, acrossSize, glsl, GLSLstd450FAbs, across});
            append(expanded, spv::OpExtInst, {type, downSize, glsl, GLSLstd450FAbs, down});
            append(expanded, spv::OpFAdd, {type, words[position + 2], acrossSize, downSize});
        } else {
            expanded.insert(expanded.end(), words.begin() + static_cast<std::ptrdiff_t>(position), words.begin() + static_cast<std::ptrdiff_t>(position + count));
        }
    }
    expanded[kBoundWord] = bound;
    return expanded;
}

std::string SpirvPatcher::readLiteral(const std::vector<std::uint32_t>& words, std::size_t position, std::size_t count) {
    // A literal string packs four characters into every word, the first in the lowest byte, and ends with a null character.
    std::string text;
    for (std::size_t index = 0; index < count * 4; ++index) {
        const auto character = static_cast<char>(words[position + index / 4] >> (8 * (index % 4)));
        if (character == '\0') {
            break;
        }
        text += character;
    }
    return text;
}

void SpirvPatcher::append(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::initializer_list<std::uint32_t> operands) {
    words.push_back(static_cast<std::uint32_t>(operands.size() + 1) << 16 | opcode);
    words.insert(words.end(), operands);
}

} // namespace haylen::shaderc
