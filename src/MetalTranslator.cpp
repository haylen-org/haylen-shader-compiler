#include "MetalTranslator.hpp"

#include <spirv_msl.hpp>

#include "TargetBindings.hpp"

namespace haylen::shaderc {

Translation MetalTranslator::translate(const Package::Program& program, const StageCompiler::Result& compiled, const ShaderSource& source, const ShaderSource::Program& sourceProgram, Package::Target target) {
    Translation translation{.code = {.target = target}};
    TargetBindings::assignCompact(program, translation.code);
    translation.vertexSource = translateStage(compiled.vertex, translation.code, translation.code.vertex, source, source.stages[sourceProgram.vertex]);
    translation.fragmentSource = translateStage(compiled.fragment, translation.code, translation.code.fragment, source, source.stages[sourceProgram.fragment]);
    return translation;
}

std::string MetalTranslator::translateStage(const std::vector<std::uint32_t>& words, Package::Code& code, Package::Function& function, const ShaderSource& source, const ShaderSource::Stage& stage) {
    try {
        spirv_cross::CompilerMSL compiler(words);
        spirv_cross::CompilerMSL::Options options = compiler.get_msl_options();
        options.platform = code.target == Package::Target::MslMacos ? spirv_cross::CompilerMSL::Options::macOS : spirv_cross::CompilerMSL::Options::iOS;
        options.set_msl_version(kVersionMajor, kVersionMinor);
        options.enable_decoration_binding = true;
        compiler.set_msl_options(options);
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
