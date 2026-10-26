#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "Declarations.hpp"
#include "ShaderSource.hpp"
#include "haylen/shaderc/Diagnostic.hpp"

namespace haylen::shaderc {

// Checks the declarations of the two stages of a program against the language, against the tags of the stages and against each other.
class DeclarationChecker {
  public:
    // A program for a GLSL target also needs uniform blocks that flatten into one array.
    static void check(const Declarations& vertex, const Declarations& fragment, const ShaderSource::Program& program, bool flattenBlocks, const ShaderSource& source, std::vector<Diagnostic>& diagnostics);

  private:
    // A binding of the program, for the check that no two declarations of one kind share it.
    struct Binding {
        int slot = 0;
        std::string name;
        ShaderSource::Location location;
    };

    struct Slots {
        std::vector<Binding> blocks;
        std::vector<Binding> textures;
        std::vector<Binding> samplers;
    };

    static const std::array<std::string_view, 9> kMemberTypes;
    static const std::array<std::string_view, 4> kTextureTypes;

    static void checkStage(const Declarations& declarations, const ShaderSource::Stage& stage, bool flattenBlocks, const ShaderSource& source, Slots& slots, std::vector<Diagnostic>& diagnostics);
    static void checkBlock(const Declarations::Block& block, bool flattenBlocks, const ShaderSource& source, std::vector<Diagnostic>& diagnostics);
    static void checkTags(const Declarations& declarations, const ShaderSource::Stage& stage, const ShaderSource& source, std::vector<Diagnostic>& diagnostics);
    [[nodiscard]] static bool checkBinding(std::string_view kind, const std::string& name, int binding, int slots, const ShaderSource::Location& location, const ShaderSource& source, std::vector<Diagnostic>& diagnostics);
    static void checkUnique(std::string_view kind, const std::vector<Binding>& bindings, const ShaderSource& source, std::vector<Diagnostic>& diagnostics);
};

} // namespace haylen::shaderc
