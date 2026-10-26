#include "Reflector.hpp"

#include <algorithm>
#include <tuple>
#include <utility>

#include <spirv_cross.hpp>

namespace haylen::shaderc {

Package::Program Reflector::reflect(std::string name, const StageCompiler::Result& compiled, const ShaderSource::Program& program, const ShaderSource& source) {
    Package::Program reflected{.name = std::move(name)};
    for (const Declarations::Variable& input : compiled.vertexDeclarations.inputs) {
        reflected.attributes.push_back({.location = input.assignedLocation, .name = input.name, .type = input.type});
    }
    for (const Declarations::Variable& output : compiled.fragmentDeclarations.outputs) {
        reflected.outputs = std::max(reflected.outputs, output.assignedLocation + output.locationSize);
    }
    reflectStage(reflected, compiled.vertex, compiled.vertexDeclarations, source.stages[program.vertex]);
    reflectStage(reflected, compiled.fragment, compiled.fragmentDeclarations, source.stages[program.fragment]);

    std::ranges::sort(reflected.attributes, {}, &Package::Attribute::location);
    std::ranges::sort(reflected.blocks, {}, &Package::Block::slot);
    std::ranges::sort(reflected.textures, {}, &Package::Texture::slot);
    std::ranges::sort(reflected.samplers, {}, &Package::Sampler::slot);
    std::ranges::sort(reflected.pairs, [](const Package::Pair& left, const Package::Pair& right) { return std::tuple(left.stage, left.texture, left.sampler) < std::tuple(right.stage, right.texture, right.sampler); });
    return reflected;
}

void Reflector::reflectStage(Package::Program& reflected, const std::vector<std::uint32_t>& words, const Declarations& declarations, const ShaderSource::Stage& stage) {
    spirv_cross::Compiler compiler(words);
    const spirv_cross::ShaderResources resources = compiler.get_shader_resources();

    // The members keep the names and types of the source, and SPIR-V gives their std140 offsets and the size of the block, which takes whole vectors of 16 bytes.
    for (const spirv_cross::Resource& resource : resources.uniform_buffers) {
        const auto slot = static_cast<int>(compiler.get_decoration(resource.id, spv::DecorationBinding));
        const Declarations::Block& block = *std::ranges::find(declarations.blocks, slot, &Declarations::Block::binding);
        const spirv_cross::SPIRType& type = compiler.get_type(resource.base_type_id);
        const std::size_t size = (compiler.get_declared_struct_size(type) + 15) / 16 * 16;
        Package::Block reflectedBlock{.name = block.name, .stage = stage.kind, .slot = slot, .size = static_cast<int>(size)};
        for (std::size_t index = 0; index < block.members.size(); ++index) {
            const Declarations::Member& member = block.members[index];
            const auto offset = static_cast<int>(compiler.type_struct_member_offset(type, static_cast<std::uint32_t>(index)));
            reflectedBlock.members.push_back({.name = member.name, .type = member.type, .count = member.count, .offset = offset});
        }
        reflected.blocks.push_back(std::move(reflectedBlock));
    }

    for (const Declarations::Texture& texture : declarations.textures) {
        const auto tag = std::ranges::find(stage.sampleTags, texture.name, &ShaderSource::SampleTag::texture);
        const Package::SampleType sampleType = tag == stage.sampleTags.end() ? texture.sampleType : tag->type;
        reflected.textures.push_back({.name = texture.name, .stage = stage.kind, .slot = texture.binding, .type = *texture.type, .sampleType = sampleType});
    }
    for (const Declarations::Sampler& sampler : declarations.samplers) {
        const auto tag = std::ranges::find(stage.samplerTags, sampler.name, &ShaderSource::SamplerTag::sampler);
        const Package::SamplerType declared = sampler.shadow ? Package::SamplerType::Comparison : Package::SamplerType::Filtering;
        reflected.samplers.push_back({.name = sampler.name, .stage = stage.kind, .slot = sampler.binding, .type = tag == stage.samplerTags.end() ? declared : tag->type});
    }

    compiler.build_combined_image_samplers();
    for (const spirv_cross::CombinedImageSampler& combined : compiler.get_combined_image_samplers()) {
        const auto texture = static_cast<int>(compiler.get_decoration(combined.image_id, spv::DecorationBinding));
        const auto sampler = static_cast<int>(compiler.get_decoration(combined.sampler_id, spv::DecorationBinding));
        const std::string name = compiler.get_name(combined.image_id) + "_" + compiler.get_name(combined.sampler_id);
        reflected.pairs.push_back({.stage = stage.kind, .texture = texture, .sampler = sampler, .name = name});
    }
}

} // namespace haylen::shaderc
