#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "haylen/shaderc/Compiler.hpp"
#include "haylen/shaderc/Package.hpp"
#include "support/TemporaryFolder.hpp"

namespace haylen::shaderc::test {

// Compiles sources that a test writes into a temporary folder, and shows their errors with paths relative to that folder.
class CompilerFixture : public ::testing::Test {
  protected:
    TemporaryFolder folder;
    Compiler compiler;

    // Writes the source as "shader.glsl" and compiles it with the options, whose input it sets.
    Package compile(std::string_view text, Compiler::Options options = {}) const;

    // Writes the source as "shader.glsl", compiles it, expects it to fail and returns its errors as the command prints them.
    std::vector<std::string> errors(std::string_view text, Compiler::Options options = {}) const;

    std::filesystem::path write(const std::filesystem::path& relative, std::string_view text) const;
};

} // namespace haylen::shaderc::test
