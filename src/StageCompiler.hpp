#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "Declarations.hpp"
#include "ShaderSource.hpp"
#include "haylen/shaderc/Diagnostic.hpp"

namespace glslang {
class TShader;
} // namespace glslang

namespace haylen::shaderc {

// Compiles the two stages of a program into SPIR-V with glslang, with the locations of their inputs and outputs assigned, their declarations checked, and the vertex outputs that the fragment stage never declares kept inside the vertex stage.
class StageCompiler {
  public:
    struct Result {
        std::vector<std::uint32_t> vertex;
        std::vector<std::uint32_t> fragment;
        Declarations vertexDeclarations;
        Declarations fragmentDeclarations;
    };

    // Sets up glslang for the process, once for every call to `finalizeProcess`.
    static void initializeProcess();
    static void finalizeProcess();

    // Compiles a program with the defines of a variant. A program for a GLSL target also needs uniform blocks that flatten. Throws `CompileError`.
    [[nodiscard]] static Result compile(const ShaderSource& source, const ShaderSource::Program& program, const std::vector<std::string>& defines, bool flattenBlocks);

  private:
    class InterfaceWriter;
    class PoolReset;

    [[nodiscard]] static bool parse(glslang::TShader& shader, const ShaderSource::Stage& stage, const std::string& preamble, const ShaderSource& source, std::vector<Diagnostic>& diagnostics);
    static void writeInterface(glslang::TShader& shader, const Declarations& declarations, std::set<long long> unread);
    [[nodiscard]] static std::set<long long> findUnread(const Declarations& vertex, const Declarations& fragment);
    [[nodiscard]] static std::vector<std::uint32_t> generate(glslang::TShader& shader, const ShaderSource::Stage& stage, const ShaderSource& source);
    [[nodiscard]] static std::string render(const ShaderSource::Stage& stage);
    [[nodiscard]] static std::string makePreamble(const std::vector<std::string>& defines);
};

} // namespace haylen::shaderc
