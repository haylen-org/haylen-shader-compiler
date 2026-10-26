#include "WgslTranslator.hpp"

#include <src/tint/api/tint.h>
#include <src/tint/lang/core/ir/module.h>
#include <src/tint/lang/spirv/reader/reader.h>
#include <src/tint/lang/wgsl/inspector/inspector.h>
#include <src/tint/lang/wgsl/program/program.h>
#include <src/tint/lang/wgsl/writer/writer.h>

#include "SpirvPatcher.hpp"
#include "TargetBindings.hpp"

namespace haylen::shaderc {

void WgslTranslator::initializeProcess() {
    tint::Initialize();
}

Translation WgslTranslator::translate(const Package::Program& program, const StageCompiler::Result& compiled, const ShaderSource& source, const ShaderSource::Program& sourceProgram) {
    Translation translation{.code = {.target = Package::Target::Wgsl}};
    TargetBindings::assignSets(program, translation.code);
    translation.vertexSource = translateStage(compiled.vertex, translation.code, translation.code.vertex, source, source.stages[sourceProgram.vertex]);
    translation.fragmentSource = translateStage(compiled.fragment, translation.code, translation.code.fragment, source, source.stages[sourceProgram.fragment]);
    return translation;
}

std::string WgslTranslator::translateStage(const std::vector<std::uint32_t>& words, const Package::Code& code, Package::Function& function, const ShaderSource& source, const ShaderSource::Stage& stage) {
    tint::Result<tint::core::ir::Module> module = tint::spirv::reader::ReadIR(SpirvPatcher::refineDerivatives(TargetBindings::rebind(words, code)));
    if (module != tint::Success) {
        throw Translation::fail(source, stage, code.target, "Tint", module.Failure().reason);
    }

    // Derivatives keep the rules of GLSL, which allow them in control flow that differs between invocations.
    tint::wgsl::writer::Options options;
    options.allow_non_uniform_derivatives = true;
    tint::Result<tint::Program> program = tint::wgsl::writer::ProgramFromIR(module.Get(), options);
    if (program != tint::Success) {
        throw Translation::fail(source, stage, code.target, "Tint", program.Failure().reason);
    }
    tint::Result<tint::wgsl::writer::Output> output = tint::wgsl::writer::Generate(program.Get(), options);
    if (output != tint::Success) {
        throw Translation::fail(source, stage, code.target, "Tint", output.Failure().reason);
    }

    tint::inspector::Inspector inspector(program.Get());
    function.entry = inspector.GetEntryPoints().front().name;
    return output->wgsl;
}

} // namespace haylen::shaderc
