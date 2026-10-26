#pragma once

#include <string>

namespace haylen::shaderc {

// An error in a source file, at a line and a column of it when the error has them.
struct Diagnostic {
    std::string file;
    int line = 0;
    int column = 0;
    std::string message;

    // Formats the error as "file:line:column: error: message", or as "file: error: message" without a line.
    [[nodiscard]] std::string toString() const;

    friend bool operator==(const Diagnostic&, const Diagnostic&) = default;
};

} // namespace haylen::shaderc
