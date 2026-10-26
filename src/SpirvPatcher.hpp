#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace haylen::shaderc {

// Rewrites the words of a SPIR-V module for the readers that take no binding remap and for the targets whose plain instructions differ from the ones of GLSL.
class SpirvPatcher {
  public:
    struct Binding {
        std::uint32_t set = 0;
        std::uint32_t binding = 0;
    };

    // Returns the module with the descriptor set and the binding of every variable of the map replaced by the ones it gives.
    [[nodiscard]] static std::vector<std::uint32_t> rebind(std::vector<std::uint32_t> words, const std::map<std::uint32_t, Binding>& bindings);

    // Returns the module with "dFdx", "dFdy" and "fwidth" made fine, since the plain derivatives of Direct3D and WebGPU may be coarse while the other graphics APIs compute them for every pixel. The coarse forms stay coarse.
    [[nodiscard]] static std::vector<std::uint32_t> refineDerivatives(std::vector<std::uint32_t> words);

    // Returns the module with every fine fwidth written as the sum of the absolute fine derivatives, for HLSL, which has no fine fwidth.
    [[nodiscard]] static std::vector<std::uint32_t> expandFineWidths(const std::vector<std::uint32_t>& words);

  private:
    static constexpr std::size_t kHeaderWords = 5;
    static constexpr std::size_t kBoundWord = 3;

    [[nodiscard]] static std::string readLiteral(const std::vector<std::uint32_t>& words, std::size_t position, std::size_t count);
    static void append(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::initializer_list<std::uint32_t> operands);
};

} // namespace haylen::shaderc
