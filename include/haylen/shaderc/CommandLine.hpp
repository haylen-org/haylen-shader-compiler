#pragma once

#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "haylen/shaderc/Compiler.hpp"

namespace haylen::shaderc {

// The command "haylen-shaderc", which compiles one source into one package.
class CommandLine {
  public:
    static constexpr int kSuccess = 0;
    static constexpr int kCompileFailed = 1;
    static constexpr int kBadArguments = 2;

    // Runs the command with the arguments that follow the name of the program and returns its exit status.
    static int run(std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err);

  private:
    struct Arguments {
        Compiler::Options options;
        std::filesystem::path output;
    };

    // The arguments, or the sentence that says what is wrong with them.
    struct Parsed {
        std::optional<Arguments> arguments;
        std::string error;
    };

    [[nodiscard]] static Parsed parse(std::span<const std::string_view> arguments);
    [[nodiscard]] static std::optional<std::string> parseVariant(std::string_view value, Compiler::Variant& variant);
    [[nodiscard]] static std::string usage();
};

} // namespace haylen::shaderc
