#include "haylen/shaderc/CompileError.hpp"

#include <algorithm>
#include <utility>

namespace haylen::shaderc {

CompileError::CompileError(std::vector<Diagnostic> value) : std::runtime_error(describe(unique(value))), diagnostics(unique(std::move(value))) {}

CompileError::CompileError(Diagnostic value) : CompileError(std::vector<Diagnostic>{std::move(value)}) {}

const std::vector<Diagnostic>& CompileError::getDiagnostics() const noexcept {
    return diagnostics;
}

std::vector<Diagnostic> CompileError::unique(std::vector<Diagnostic> value) {
    std::vector<Diagnostic> kept;
    kept.reserve(value.size());
    for (Diagnostic& diagnostic : value) {
        if (std::ranges::find(kept, diagnostic) == kept.end()) {
            kept.push_back(std::move(diagnostic));
        }
    }
    return kept;
}

std::string CompileError::describe(const std::vector<Diagnostic>& value) {
    std::string text;
    for (const Diagnostic& diagnostic : value) {
        if (!text.empty()) {
            text += '\n';
        }
        text += diagnostic.toString();
    }
    return text;
}

} // namespace haylen::shaderc
