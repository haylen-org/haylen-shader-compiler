#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ShaderSource.hpp"
#include "StageCompiler.hpp"
#include "Translation.hpp"
#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

// Translates programs into the Metal Shading Language of macOS or iOS with SPIRV-Cross. Vertex inputs arrive through "[[stage_in]]" at their locations, and the uniform blocks, textures and samplers of each stage bind from 0 in the order of their slots.
class MetalTranslator {
  public:
    static constexpr int kVersionMajor = 2;
    static constexpr int kVersionMinor = 0;

    // Throws `CompileError`.
    [[nodiscard]] static Translation translate(const Package::Program& program, const StageCompiler::Result& compiled, const ShaderSource& source, const ShaderSource::Program& sourceProgram, Package::Target target);

  private:
    [[nodiscard]] static std::string translateStage(const std::vector<std::uint32_t>& words, Package::Code& code, Package::Function& function, const ShaderSource& source, const ShaderSource::Stage& stage);
};

} // namespace haylen::shaderc
