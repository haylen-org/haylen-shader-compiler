#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/shaderc/Diagnostic.hpp"

namespace haylen::shaderc {

// The errors that stop a compilation, each once, in the order they were found.
class CompileError : public std::runtime_error {
  public:
    explicit CompileError(std::vector<Diagnostic> value);
    explicit CompileError(Diagnostic value);

    [[nodiscard]] const std::vector<Diagnostic>& getDiagnostics() const noexcept;

  private:
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] static std::vector<Diagnostic> unique(std::vector<Diagnostic> value);
    [[nodiscard]] static std::string describe(const std::vector<Diagnostic>& value);
};

} // namespace haylen::shaderc
