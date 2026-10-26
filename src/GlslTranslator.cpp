#include "GlslTranslator.hpp"

#include <algorithm>
#include <format>

#include <spirv_glsl.hpp>

namespace haylen::shaderc {

Translation GlslTranslator::translate(const Package::Program& program, const StageCompiler::Result& compiled, const ShaderSource& source, const ShaderSource::Program& sourceProgram, Package::Target target) {
    Translation translation{.code = {.target = target}};
    translation.vertexSource = translateStage(compiled.vertex, program, translation.code, source, source.stages[sourceProgram.vertex]);
    translation.fragmentSource = translateStage(compiled.fragment, program, translation.code, source, source.stages[sourceProgram.fragment]);
    translation.code.vertex.entry = "main";
    translation.code.fragment.entry = "main";
    return translation;
}

std::string GlslTranslator::translateStage(const std::vector<std::uint32_t>& words, const Package::Program& program, Package::Code& code, const ShaderSource& source, const ShaderSource::Stage& stage) {
    try {
        spirv_cross::CompilerGLSL compiler(words);
        spirv_cross::CompilerGLSL::Options options = compiler.get_common_options();
        const bool embedded = code.target == Package::Target::Glsl300es;
        options.version = embedded ? kEmbeddedVersion : kDesktopVersion;
        options.es = embedded;
        options.enable_420pack_extension = false;
        options.vertex.fixup_clipspace = true;
        options.fragment.default_float_precision = spirv_cross::CompilerGLSL::Options::Highp;
        options.fragment.default_int_precision = spirv_cross::CompilerGLSL::Options::Highp;
        compiler.set_common_options(options);

        const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
        for (const spirv_cross::Resource& resource : resources.uniform_buffers) {
            compiler.flatten_buffer_block(resource.id);
        }

        // A renderer binds every combined sampler to a texture unit of its own by name, so the samplers keep no bindings.
        compiler.build_combined_image_samplers();
        for (const spirv_cross::CombinedImageSampler& combined : compiler.get_combined_image_samplers()) {
            compiler.set_name(combined.combined_id, compiler.get_name(combined.image_id) + "_" + compiler.get_name(combined.sampler_id));
            compiler.unset_decoration(combined.combined_id, spv::DecorationBinding);
            compiler.unset_decoration(combined.combined_id, spv::DecorationDescriptorSet);
        }
        std::string text = compiler.compile();

        for (const spirv_cross::CombinedImageSampler& combined : compiler.get_combined_image_samplers()) {
            const std::string& texture = compiler.get_name(combined.image_id);
            const std::string& sampler = compiler.get_name(combined.sampler_id);
            if (compiler.get_name(combined.combined_id) != texture + "_" + sampler) {
                throw CompileError(source.diagnose(stage.location, std::format("The texture \"{}\" and the sampler \"{}\" combine into \"{}_{}\", which the GLSL targets cannot name. Rename the texture or the sampler.", texture, sampler, texture, sampler)));
            }
        }
        for (const spirv_cross::Resource& resource : resources.uniform_buffers) {
            const auto slot = static_cast<int>(compiler.get_decoration(resource.id, spv::DecorationBinding));
            const Package::Block& block = *std::ranges::find(program.blocks, slot, &Package::Block::slot);
            const bool integers = std::ranges::all_of(block.members, [](const Package::Member& member) { return member.type.starts_with('i'); });
            code.flatBlocks[slot] = {.name = compiler.get_name(resource.base_type_id), .type = integers ? "ivec4" : "vec4", .count = block.size / 16};
        }
        return text;
    } catch (const spirv_cross::CompilerError& error) {
        throw Translation::fail(source, stage, code.target, "SPIRV-Cross", error.what());
    }
}

} // namespace haylen::shaderc
