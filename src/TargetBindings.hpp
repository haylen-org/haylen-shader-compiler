#pragma once

#include <cstdint>
#include <vector>

#include "haylen/shaderc/Package.hpp"

namespace spirv_cross {
class Compiler;
} // namespace spirv_cross

namespace haylen::shaderc {

// The bindings that the targets give the uniform block, texture and sampler slots of a program.
class TargetBindings {
  public:
    static constexpr int kBlockSet = 0;
    static constexpr int kResourceSet = 1;
    static constexpr int kSamplerOffset = 64;

    // Metal numbers the uniform blocks, the textures and the samplers of each stage from 0 in the order of their slots, which keeps textures and samplers within the 31 textures and 16 samplers of the oldest Apple GPUs.
    static void assignCompact(const Package::Program& program, Package::Code& code);

    // HLSL numbers the uniform blocks of each stage from 0 in the order of their slots, and binds textures and samplers at their slots.
    static void assignPerStage(const Package::Program& program, Package::Code& code);

    // SPIR-V and WGSL bind uniform blocks in set 0 at their slots, and textures and samplers in set 1, the samplers 64 after their slots.
    static void assignSets(const Package::Program& program, Package::Code& code);

    // Decorates the uniform blocks, textures and samplers of a stage with the bindings of the code, for the translators of SPIRV-Cross.
    static void apply(spirv_cross::Compiler& compiler, const Package::Code& code);

    // Returns the words of a stage with the descriptor set and the binding of every uniform block, texture and sampler replaced by the ones of the code.
    [[nodiscard]] static std::vector<std::uint32_t> rebind(const std::vector<std::uint32_t>& words, const Package::Code& code);
};

} // namespace haylen::shaderc
