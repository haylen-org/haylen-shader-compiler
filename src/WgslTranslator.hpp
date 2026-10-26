#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ShaderSource.hpp"
#include "StageCompiler.hpp"
#include "Translation.hpp"
#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

// Translates programs into WGSL with Tint, which reads the SPIR-V of every stage and writes WGSL. Uniform blocks bind in group 0 at their slots, and textures and samplers in group 1, the samplers 64 after their slots. Derivatives are fine, as on the other graphics APIs, except the coarse forms of GLSL.
class WgslTranslator {
  public:
    // Sets up Tint for the process.
    static void initializeProcess();

    // Throws `CompileError`.
    [[nodiscard]] static Translation translate(const Package::Program& program, const StageCompiler::Result& compiled, const ShaderSource& source, const ShaderSource::Program& sourceProgram);

  private:
    [[nodiscard]] static std::string translateStage(const std::vector<std::uint32_t>& words, const Package::Code& code, Package::Function& function, const ShaderSource& source, const ShaderSource::Stage& stage);
};

} // namespace haylen::shaderc
