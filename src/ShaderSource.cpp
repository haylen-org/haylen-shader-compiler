#include "ShaderSource.hpp"

#include <utility>

namespace haylen::shaderc {

Diagnostic ShaderSource::diagnose(const Location& location, std::string message) const {
    return {.file = files.at(static_cast<std::size_t>(location.file)), .line = location.line, .column = location.column, .message = std::move(message)};
}

} // namespace haylen::shaderc
