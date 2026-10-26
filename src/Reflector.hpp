#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Declarations.hpp"
#include "ShaderSource.hpp"
#include "StageCompiler.hpp"
#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

// Reflects a compiled program into the package: its vertex attributes, uniform blocks with their std140 layout, textures, samplers, the texture and sampler pairs that the stages sample with, and its color outputs.
class Reflector {
  public:
    [[nodiscard]] static Package::Program reflect(std::string name, const StageCompiler::Result& compiled, const ShaderSource::Program& program, const ShaderSource& source);

  private:
    static void reflectStage(Package::Program& reflected, const std::vector<std::uint32_t>& words, const Declarations& declarations, const ShaderSource::Stage& stage);
};

} // namespace haylen::shaderc
