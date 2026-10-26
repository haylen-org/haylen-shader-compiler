#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Declarations.hpp"
#include "ShaderSource.hpp"
#include "haylen/shaderc/Diagnostic.hpp"

namespace glslang {
class TIntermediate;
class TType;
struct TSampler;
struct TSourceLoc;
} // namespace glslang

namespace haylen::shaderc {

// Reads the global declarations of a parsed stage from the syntax tree of glslang.
class DeclarationReader {
  public:
    // Adds an error for every declaration that the language does not support.
    [[nodiscard]] static Declarations read(const glslang::TIntermediate& intermediate, const ShaderSource::Stage& stage, const ShaderSource& source, std::vector<Diagnostic>& diagnostics);

  private:
    class PlaceFinder;

    static void readUniform(Declarations& declarations, const glslang::TType& type, const std::string& name, const ShaderSource::Location& location, const ShaderSource::Stage& stage, const ShaderSource& source, std::vector<Diagnostic>& diagnostics);
    [[nodiscard]] static ShaderSource::Location toLocation(const glslang::TSourceLoc& place, const ShaderSource::Stage& stage, const ShaderSource& source);
    [[nodiscard]] static std::string typeName(const glslang::TType& type);
    [[nodiscard]] static std::string elementName(const glslang::TType& type);
    [[nodiscard]] static std::optional<Package::TextureType> textureType(const glslang::TSampler& sampler);
    [[nodiscard]] static bool isBuiltInBlock(const glslang::TType& type);
};

} // namespace haylen::shaderc
