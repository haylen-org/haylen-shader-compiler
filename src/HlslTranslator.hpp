#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ShaderSource.hpp"
#include "StageCompiler.hpp"
#include "Translation.hpp"
#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

// Translates programs into HLSL for shader model 5.0 with SPIRV-Cross. Vertex inputs take the semantic "TEXCOORD" with their location as its index, uniform blocks bind to the "b" registers of their stage, and textures and samplers to the "t" and "s" registers of their slots. Derivatives are fine, as on the other graphics APIs, except the coarse forms of GLSL.
class HlslTranslator {
  public:
    static constexpr int kShaderModel = 50;

    // Throws `CompileError`.
    [[nodiscard]] static Translation translate(const Package::Program& program, const StageCompiler::Result& compiled, const ShaderSource& source, const ShaderSource::Program& sourceProgram);

  private:
    [[nodiscard]] static std::string translateStage(const std::vector<std::uint32_t>& words, const Package::Code& code, Package::Function& function, const ShaderSource& source, const ShaderSource::Stage& stage);
};

} // namespace haylen::shaderc
