#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "StageCompiler.hpp"
#include "Translation.hpp"
#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

// Writes programs as SPIR-V 1.0 modules for Vulkan 1.1: the modules of glslang with the bindings of the target, as base64 text. Uniform blocks bind in set 0 at their slots, and textures and samplers in set 1, the samplers 64 after their slots, while the vertex inputs keep their locations.
class SpirvTranslator {
  public:
    [[nodiscard]] static Translation translate(const Package::Program& program, const StageCompiler::Result& compiled);

  private:
    static constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    [[nodiscard]] static std::string encode(const std::vector<std::uint32_t>& words);
};

} // namespace haylen::shaderc
