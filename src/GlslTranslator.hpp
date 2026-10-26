#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ShaderSource.hpp"
#include "StageCompiler.hpp"
#include "Translation.hpp"
#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

// Translates programs into GLSL 4.30 or GLSL ES 3.00 with SPIRV-Cross. Every uniform block becomes one array of vectors named after the block, every texture and sampler pair one combined sampler named after both, and the depth of clip space follows OpenGL.
class GlslTranslator {
  public:
    static constexpr int kDesktopVersion = 430;
    static constexpr int kEmbeddedVersion = 300;

    // Throws `CompileError`.
    [[nodiscard]] static Translation translate(const Package::Program& program, const StageCompiler::Result& compiled, const ShaderSource& source, const ShaderSource::Program& sourceProgram, Package::Target target);

  private:
    [[nodiscard]] static std::string translateStage(const std::vector<std::uint32_t>& words, const Package::Program& program, Package::Code& code, const ShaderSource& source, const ShaderSource::Stage& stage);
};

} // namespace haylen::shaderc
