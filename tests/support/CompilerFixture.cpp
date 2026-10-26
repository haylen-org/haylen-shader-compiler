#include "support/CompilerFixture.hpp"

#include <utility>

#include "haylen/shaderc/CompileError.hpp"

namespace haylen::shaderc::test {

Package CompilerFixture::compile(std::string_view text, Compiler::Options options) const {
    options.input = write("shader.glsl", text);
    return compiler.compile(options);
}

std::vector<std::string> CompilerFixture::errors(std::string_view text, Compiler::Options options) const {
    options.input = write("shader.glsl", text);
    const std::string prefix = folder.getPath().generic_string() + "/";
    std::vector<std::string> found;
    try {
        static_cast<void>(compiler.compile(options));
        ADD_FAILURE() << "The source compiled, but the test expects errors.";
    } catch (const CompileError& error) {
        for (const Diagnostic& diagnostic : error.getDiagnostics()) {
            std::string line = diagnostic.toString();
            for (std::size_t place = line.find(prefix); place != std::string::npos; place = line.find(prefix, place)) {
                line.erase(place, prefix.size());
            }
            found.push_back(std::move(line));
        }
    }
    return found;
}

std::filesystem::path CompilerFixture::write(const std::filesystem::path& relative, std::string_view text) const {
    return folder.write(relative, text);
}

} // namespace haylen::shaderc::test
