#include "StageCompiler.hpp"

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <utility>

#include <SPIRV/GlslangToSpv.h>
#include <glslang/MachineIndependent/localintermediate.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include "DeclarationChecker.hpp"
#include "DeclarationReader.hpp"
#include "GlslangLog.hpp"
#include "LocationAssigner.hpp"
#include "haylen/shaderc/CompileError.hpp"

namespace haylen::shaderc {

// Writes the assigned locations into every symbol of the inputs and outputs of a stage, which SPIR-V then decorates with them, and turns the outputs that the next stage never declares into private variables, so the interfaces of both stages hold the same variables in the order of their locations.
class StageCompiler::InterfaceWriter : public glslang::TIntermTraverser {
  public:
    InterfaceWriter(const Declarations& declarations, std::set<long long> unread) : glslang::TIntermTraverser(true, false, false, false, true), unread(std::move(unread)) {
        for (const std::vector<Declarations::Variable>* variables : {&declarations.inputs, &declarations.outputs}) {
            for (const Declarations::Variable& variable : *variables) {
                locations.emplace(variable.id, variable.assignedLocation);
            }
        }
    }

    void visitSymbol(glslang::TIntermSymbol* symbol) override {
        glslang::TQualifier& qualifier = symbol->getWritableType().getQualifier();
        if (unread.contains(symbol->getId())) {
            qualifier.storage = glslang::EvqGlobal;
            qualifier.invariant = false;
            qualifier.clearInterstage();
            qualifier.clearInterstageLayout();
            return;
        }
        const auto found = locations.find(symbol->getId());
        if (found != locations.end()) {
            qualifier.layoutLocation = static_cast<unsigned int>(found->second) & glslang::TQualifier::layoutLocationEnd;
        }
    }

  private:
    std::map<long long, int> locations;
    std::set<long long> unread;
};

// The shaders and programs of glslang point the thread at their own pools of memory and leave it pointing there once they are gone, so every compilation points it back at the pool of the thread when it ends.
class StageCompiler::PoolReset {
  public:
    PoolReset() = default;
    PoolReset(const PoolReset&) = delete;
    PoolReset& operator=(const PoolReset&) = delete;

    ~PoolReset() {
        glslang::SetThreadPoolAllocator(nullptr);
    }
};

void StageCompiler::initializeProcess() {
    glslang::InitializeProcess();
}

void StageCompiler::finalizeProcess() {
    glslang::FinalizeProcess();
}

StageCompiler::Result StageCompiler::compile(const ShaderSource& source, const ShaderSource::Program& program, const std::vector<std::string>& defines, bool flattenBlocks) {
    const ShaderSource::Stage& vertexStage = source.stages[program.vertex];
    const ShaderSource::Stage& fragmentStage = source.stages[program.fragment];
    const std::string preamble = makePreamble(defines);

    const PoolReset reset;
    std::vector<Diagnostic> diagnostics;
    glslang::TShader vertex(EShLangVertex);
    glslang::TShader fragment(EShLangFragment);
    const bool vertexParsed = parse(vertex, vertexStage, preamble, source, diagnostics);
    const bool fragmentParsed = parse(fragment, fragmentStage, preamble, source, diagnostics);
    if (!vertexParsed || !fragmentParsed) {
        throw CompileError(std::move(diagnostics));
    }

    Result result{
        .vertexDeclarations = DeclarationReader::read(*vertex.getIntermediate(), vertexStage, source, diagnostics),
        .fragmentDeclarations = DeclarationReader::read(*fragment.getIntermediate(), fragmentStage, source, diagnostics),
    };
    DeclarationChecker::check(result.vertexDeclarations, result.fragmentDeclarations, program, flattenBlocks, source, diagnostics);
    LocationAssigner::assign(result.vertexDeclarations, result.fragmentDeclarations, source, diagnostics);
    if (!diagnostics.empty()) {
        throw CompileError(std::move(diagnostics));
    }

    writeInterface(vertex, result.vertexDeclarations, findUnread(result.vertexDeclarations, result.fragmentDeclarations));
    writeInterface(fragment, result.fragmentDeclarations, {});
    result.vertex = generate(vertex, vertexStage, source);
    result.fragment = generate(fragment, fragmentStage, source);
    return result;
}

bool StageCompiler::parse(glslang::TShader& shader, const ShaderSource::Stage& stage, const std::string& preamble, const ShaderSource& source, std::vector<Diagnostic>& diagnostics) {
    const std::string rendered = render(stage);
    const char* text = rendered.c_str();
    const EShLanguage language = stage.kind == Package::Stage::Vertex ? EShLangVertex : EShLangFragment;
    shader.setStrings(&text, 1);
    shader.setPreamble(preamble.c_str());
    shader.setEntryPoint("main");
    shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    // The compiler assigns the locations that the source leaves out once both stages are parsed, and the declaration nodes of debug information give it the place of every declaration.
    shader.setAutoMapLocations(true);
    shader.setDebugInfo(true);
    const auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDisplayErrorColumn);
    if (shader.parse(GetDefaultResources(), 450, false, messages)) {
        return true;
    }

    std::vector<Diagnostic> found = GlslangLog::parse(shader.getInfoLog(), stage, source);
    if (found.empty()) {
        found.push_back(source.diagnose(stage.location, std::format("The {} stage \"{}\" does not compile.", Package::stageName(stage.kind), stage.name)));
    }
    diagnostics.insert(diagnostics.end(), found.begin(), found.end());
    return false;
}

void StageCompiler::writeInterface(glslang::TShader& shader, const Declarations& declarations, std::set<long long> unread) {
    InterfaceWriter writer(declarations, std::move(unread));
    shader.getIntermediate()->getTreeRoot()->traverse(&writer);
}

std::set<long long> StageCompiler::findUnread(const Declarations& vertex, const Declarations& fragment) {
    // Direct3D links the stages by the order of their variables, so a vertex output that the fragment stage skips would shift every input after it.
    std::set<long long> unread;
    for (const Declarations::Variable& output : vertex.outputs) {
        if (std::ranges::find(fragment.inputs, output.name, &Declarations::Variable::name) == fragment.inputs.end()) {
            unread.insert(output.id);
        }
    }
    return unread;
}

std::vector<std::uint32_t> StageCompiler::generate(glslang::TShader& shader, const ShaderSource::Stage& stage, const ShaderSource& source) {
    const PoolReset reset;
    glslang::TProgram program;
    program.addShader(&shader);
    const auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDisplayErrorColumn);
    if (!program.link(messages)) {
        std::vector<Diagnostic> found = GlslangLog::parse(program.getInfoLog(), stage, source);
        if (found.empty()) {
            found.push_back(source.diagnose(stage.location, std::format("The {} stage \"{}\" does not link.", Package::stageName(stage.kind), stage.name)));
        }
        throw CompileError(std::move(found));
    }

    glslang::SpvOptions options;
    options.disableOptimizer = true;
    std::vector<std::uint32_t> words;
    glslang::GlslangToSpv(*program.getIntermediate(shader.getStage()), words, &options);
    return words;
}

std::string StageCompiler::render(const ShaderSource::Stage& stage) {
    // A "#line" directive gives every line the index of its file as the number of its source string, and its own line number.
    std::string text = "#version 450\n";
    int file = -1;
    int next = 0;
    for (const ShaderSource::Line& line : stage.lines) {
        if (line.file != file || line.number != next) {
            text += std::format("#line {} {}\n", line.number, line.file);
            file = line.file;
        }
        text += line.text;
        text += '\n';
        next = line.number + 1;
    }
    return text;
}

std::string StageCompiler::makePreamble(const std::vector<std::string>& defines) {
    std::string preamble;
    for (const std::string& define : defines) {
        preamble += std::format("#define {} 1\n", define);
    }
    return preamble;
}

} // namespace haylen::shaderc
