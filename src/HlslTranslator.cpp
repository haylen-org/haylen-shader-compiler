#include "HlslTranslator.hpp"

#include <spirv_hlsl.hpp>

#include "SpirvPatcher.hpp"
#include "TargetBindings.hpp"

namespace haylen::shaderc {

Translation HlslTranslator::translate(const Package::Program& program, const StageCompiler::Result& compiled, const ShaderSource& source, const ShaderSource::Program& sourceProgram) {
    Translation translation{.code = {.target = Package::Target::Hlsl5}};
    TargetBindings::assignPerStage(program, translation.code);
    translation.code.vertex.profile = "vs_5_0";
    translation.code.fragment.profile = "ps_5_0";
    translation.vertexSource = translateStage(compiled.vertex, translation.code, translation.code.vertex, source, source.stages[sourceProgram.vertex]);
    translation.fragmentSource = translateStage(compiled.fragment, translation.code, translation.code.fragment, source, source.stages[sourceProgram.fragment]);
    return translation;
}

std::string HlslTranslator::translateStage(const std::vector<std::uint32_t>& words, const Package::Code& code, Package::Function& function, const ShaderSource& source, const ShaderSource::Stage& stage) {
    try {
        spirv_cross::CompilerHLSL compiler(SpirvPatcher::expandFineWidths(SpirvPatcher::refineDerivatives(words)));
        spirv_cross::CompilerHLSL::Options options = compiler.get_hlsl_options();
        options.shader_model = kShaderModel;
        // Direct3D draws every point one pixel wide, so a stage that writes the point size for the other APIs leaves it out.
        options.point_size_compat = true;
        compiler.set_hlsl_options(options);
        TargetBindings::apply(compiler, code);

        std::string text = compiler.compile();
        const spv::ExecutionModel model = stage.kind == Package::Stage::Vertex ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
        function.entry = compiler.get_cleansed_entry_point_name("main", model);
        return text;
    } catch (const spirv_cross::CompilerError& error) {
        throw Translation::fail(source, stage, code.target, "SPIRV-Cross", error.what());
    }
}

} // namespace haylen::shaderc
