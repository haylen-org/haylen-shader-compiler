#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ShaderSource.hpp"
#include "haylen/shaderc/Diagnostic.hpp"

namespace haylen::shaderc {

// Turns the log of glslang into diagnostics at the places of the source files, which the "#line" directives of the stage name, with its messages written as sentences.
class GlslangLog {
  public:
    // An error that names no place falls on the tag of the stage.
    [[nodiscard]] static std::vector<Diagnostic> parse(std::string_view log, const ShaderSource::Stage& stage, const ShaderSource& source);

  private:
    [[nodiscard]] static std::optional<ShaderSource::Location> readLocation(std::string_view& text);
    [[nodiscard]] static std::optional<int> readNumber(std::string_view& text);
    [[nodiscard]] static std::string toSentence(std::string_view message);
    [[nodiscard]] static std::string_view trim(std::string_view text) noexcept;
};

} // namespace haylen::shaderc
