#include "TargetBindings.hpp"

#include <array>

#include <spirv_cross.hpp>

#include "SpirvPatcher.hpp"

namespace haylen::shaderc {

void TargetBindings::assignCompact(const Package::Program& program, Package::Code& code) {
    std::array<int, 2> blocks{};
    std::array<int, 2> textures{};
    std::array<int, 2> samplers{};
    for (const Package::Block& block : program.blocks) {
        code.blocks[block.slot] = {.binding = blocks[static_cast<std::size_t>(block.stage)]++};
    }
    for (const Package::Texture& texture : program.textures) {
        code.textures[texture.slot] = {.binding = textures[static_cast<std::size_t>(texture.stage)]++};
    }
    for (const Package::Sampler& sampler : program.samplers) {
        code.samplers[sampler.slot] = {.binding = samplers[static_cast<std::size_t>(sampler.stage)]++};
    }
}

void TargetBindings::assignPerStage(const Package::Program& program, Package::Code& code) {
    std::array<int, 2> next{};
    for (const Package::Block& block : program.blocks) {
        code.blocks[block.slot] = {.binding = next[static_cast<std::size_t>(block.stage)]++};
    }
    for (const Package::Texture& texture : program.textures) {
        code.textures[texture.slot] = {.binding = texture.slot};
    }
    for (const Package::Sampler& sampler : program.samplers) {
        code.samplers[sampler.slot] = {.binding = sampler.slot};
    }
}

void TargetBindings::assignSets(const Package::Program& program, Package::Code& code) {
    for (const Package::Block& block : program.blocks) {
        code.blocks[block.slot] = {.set = kBlockSet, .binding = block.slot};
    }
    for (const Package::Texture& texture : program.textures) {
        code.textures[texture.slot] = {.set = kResourceSet, .binding = texture.slot};
    }
    for (const Package::Sampler& sampler : program.samplers) {
        code.samplers[sampler.slot] = {.set = kResourceSet, .binding = kSamplerOffset + sampler.slot};
    }
}

void TargetBindings::apply(spirv_cross::Compiler& compiler, const Package::Code& code) {
    const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
    // clang-format off
    const auto decorate = [&compiler](const spirv_cross::Resource& resource, const std::map<int, Package::Binding>& bindings) {
        const auto slot = static_cast<int>(compiler.get_decoration(resource.id, spv::DecorationBinding));
        compiler.set_decoration(resource.id, spv::DecorationBinding, static_cast<std::uint32_t>(bindings.at(slot).binding));
    };
    // clang-format on
    for (const spirv_cross::Resource& resource : resources.uniform_buffers) {
        decorate(resource, code.blocks);
    }
    for (const spirv_cross::Resource& resource : resources.separate_images) {
        decorate(resource, code.textures);
    }
    for (const spirv_cross::Resource& resource : resources.separate_samplers) {
        decorate(resource, code.samplers);
    }
}

std::vector<std::uint32_t> TargetBindings::rebind(const std::vector<std::uint32_t>& words, const Package::Code& code) {
    const spirv_cross::Compiler reflection(words);
    const spirv_cross::ShaderResources resources = reflection.get_shader_resources();
    std::map<std::uint32_t, SpirvPatcher::Binding> bindings;
    // clang-format off
    const auto add = [&reflection, &bindings](const spirv_cross::Resource& resource, const std::map<int, Package::Binding>& slots) {
        const Package::Binding& binding = slots.at(static_cast<int>(reflection.get_decoration(resource.id, spv::DecorationBinding)));
        bindings[resource.id] = {.set = static_cast<std::uint32_t>(binding.set), .binding = static_cast<std::uint32_t>(binding.binding)};
    };
    // clang-format on
    for (const spirv_cross::Resource& resource : resources.uniform_buffers) {
        add(resource, code.blocks);
    }
    for (const spirv_cross::Resource& resource : resources.separate_images) {
        add(resource, code.textures);
    }
    for (const spirv_cross::Resource& resource : resources.separate_samplers) {
        add(resource, code.samplers);
    }
    return SpirvPatcher::rebind(words, bindings);
}

} // namespace haylen::shaderc
