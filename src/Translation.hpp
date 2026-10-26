#pragma once

#include <string>
#include <string_view>

#include "ShaderSource.hpp"
#include "haylen/shaderc/CompileError.hpp"
#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

// A program translated for one target: the sources of its stages, which the package lists once each, and its code.
struct Translation {
    std::string vertexSource;
    std::string fragmentSource;
    Package::Code code;

    // The error of a target that cannot translate a stage, with the reason that the library gives.
    [[nodiscard]] static CompileError fail(const ShaderSource& source, const ShaderSource::Stage& stage, Package::Target target, std::string_view library, std::string_view reason);
};

} // namespace haylen::shaderc
