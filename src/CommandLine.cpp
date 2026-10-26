#include "haylen/shaderc/CommandLine.hpp"

#include <algorithm>
#include <format>
#include <ostream>
#include <stdexcept>

#include "Names.hpp"
#include "PathText.hpp"
#include "haylen/shaderc/CompileError.hpp"
#include "haylen/shaderc/PackageWriter.hpp"

namespace haylen::shaderc {

int CommandLine::run(std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err) {
    if (std::ranges::find(arguments, "--help") != arguments.end()) {
        out << usage();
        return kSuccess;
    }
    if (std::ranges::find(arguments, "--version") != arguments.end()) {
        out << "haylen-shaderc " << HAYLEN_SHADERC_VERSION << '\n';
        return kSuccess;
    }

    const Parsed parsed = parse(arguments);
    if (!parsed.arguments) {
        err << "haylen-shaderc: error: " << parsed.error << "\nRun \"haylen-shaderc --help\" to see how to use it.\n";
        return kBadArguments;
    }

    try {
        const Compiler compiler;
        const Package package = compiler.compile(parsed.arguments->options);
        PackageWriter::save(package, parsed.arguments->output);
    } catch (const std::invalid_argument& error) {
        err << "haylen-shaderc: error: " << error.what() << "\nRun \"haylen-shaderc --help\" to see how to use it.\n";
        return kBadArguments;
    } catch (const CompileError& error) {
        for (const Diagnostic& diagnostic : error.getDiagnostics()) {
            err << diagnostic.toString() << '\n';
        }
        return kCompileFailed;
    } catch (const std::runtime_error& error) {
        err << "haylen-shaderc: error: " << error.what() << '\n';
        return kCompileFailed;
    }
    return kSuccess;
}

CommandLine::Parsed CommandLine::parse(std::span<const std::string_view> arguments) {
    Arguments result;
    std::optional<std::filesystem::path> input;
    std::optional<std::filesystem::path> output;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string_view argument = arguments[index];
        const bool takesValue = argument == "--output" || argument == "--include" || argument == "--variant" || argument == "--target";
        if (!takesValue && argument.starts_with('-') && argument.size() > 1) {
            return {.error = std::format("The option \"{}\" does not exist.", argument)};
        }
        if (!takesValue) {
            if (input) {
                return {.error = std::format("The command takes one input file, but it is given \"{}\" and \"{}\".", PathText::of(*input), argument)};
            }
            input = PathText::from(argument);
            continue;
        }

        if (index + 1 == arguments.size() || arguments[index + 1].empty()) {
            return {.error = std::format("The option \"{}\" needs a value after it.", argument)};
        }
        const std::string_view value = arguments[++index];
        if (argument == "--output") {
            if (output) {
                return {.error = "The option \"--output\" is given twice."};
            }
            output = PathText::from(value);
        } else if (argument == "--include") {
            result.options.includes.push_back(PathText::from(value));
        } else if (argument == "--variant") {
            Compiler::Variant variant;
            if (std::optional<std::string> error = parseVariant(value, variant)) {
                return {.error = std::move(*error)};
            }
            result.options.variants.push_back(std::move(variant));
        } else {
            const std::optional<Package::Target> target = Package::targetFromName(value);
            if (!target) {
                return {.error = std::format("The target \"{}\" does not exist. The targets are {}.", value, Names::quoted(Package::kTargetNames, "and"))};
            }
            result.options.targets.push_back(*target);
        }
    }

    if (!input) {
        return {.error = "The command needs an input file, such as \"haylen-shaderc sprite.glsl --output sprite.shader\"."};
    }
    if (!output) {
        return {.error = "The command needs the package to write, which \"--output\" names."};
    }
    result.options.input = *input;
    result.output = *output;
    return {.arguments = std::move(result)};
}

std::optional<std::string> CommandLine::parseVariant(std::string_view value, Compiler::Variant& variant) {
    // A variant reads "name", "name=" or "name=DEFINE,DEFINE".
    const std::size_t equals = value.find('=');
    variant.name = std::string(value.substr(0, equals));
    if (equals == std::string_view::npos || equals + 1 == value.size()) {
        return std::nullopt;
    }

    std::string_view defines = value.substr(equals + 1);
    while (true) {
        const std::size_t comma = defines.find(',');
        const std::string_view define = defines.substr(0, comma);
        if (define.empty()) {
            return std::format("The variant \"{}\" has an empty define. Separate its defines with single commas, such as \"--variant lit=HAYLEN_LIT,HAYLEN_BATCH\".", variant.name);
        }
        variant.defines.emplace_back(define);
        if (comma == std::string_view::npos) {
            return std::nullopt;
        }
        defines.remove_prefix(comma + 1);
    }
}

std::string CommandLine::usage() {
    return std::format(R"(Usage: haylen-shaderc <input.glsl> --output <file.shader> [options]

Compiles a source of the Haylen shader language into a Haylen shader package.

Options:
  --output <file>               The package to write.
  --include <folder>            A folder where "@include" looks for files after the folder of the file that includes them. Repeat it for more folders, which it searches in order.
  --variant <name>[=DEFINE,...] Compiles the one program of the source once more as the program <name> of the package, with the defines given. Repeat it for more variants.
  --target <name>               Compiles for this target. Repeat it for more targets. Without it, every target compiles.
  --help                        Shows this help.
  --version                     Shows the version.

Targets: {}.

Exit status: 0 when the package is written, 1 when the source does not compile or the package cannot be written, and 2 when the arguments are wrong.
)",
                       Names::quoted(Package::kTargetNames, "and"));
}

} // namespace haylen::shaderc
