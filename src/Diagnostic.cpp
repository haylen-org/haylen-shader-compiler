#include "haylen/shaderc/Diagnostic.hpp"

#include <format>

namespace haylen::shaderc {

std::string Diagnostic::toString() const {
    if (line == 0) {
        return std::format("{}: error: {}", file, message);
    }
    return std::format("{}:{}:{}: error: {}", file, line, column, message);
}

} // namespace haylen::shaderc
