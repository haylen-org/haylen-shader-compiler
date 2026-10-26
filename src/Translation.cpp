#include "Translation.hpp"

#include <algorithm>
#include <format>

namespace haylen::shaderc {

CompileError Translation::fail(const ShaderSource& source, const ShaderSource::Stage& stage, Package::Target target, std::string_view library, std::string_view reason) {
    std::string flat(reason);
    std::ranges::replace(flat, '\n', ' ');
    while (!flat.empty() && flat.back() == ' ') {
        flat.pop_back();
    }
    return CompileError(source.diagnose(stage.location, std::format("The target \"{}\" cannot translate the {} stage \"{}\", and {} reports \"{}\".", Package::targetName(target), Package::stageName(stage.kind), stage.name, library, flat)));
}

} // namespace haylen::shaderc
