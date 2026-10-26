#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/shaderc/Diagnostic.hpp"
#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

struct ShaderSource;
struct Translation;

// Compiles a source of the Haylen shader language into a package with the code of every target, as `docs/language.md` describes.
class Compiler {
  public:
    // A compilation of the one program of a source with the defines of the variant, which names the program in the package.
    struct Variant {
        std::string name;
        std::vector<std::string> defines;
    };

    // Without variants every program compiles once under its own name, and without targets every target compiles. Zero threads start one thread per processor.
    struct Options {
        std::filesystem::path input;
        std::vector<std::filesystem::path> includes;
        std::vector<Variant> variants;
        std::vector<Package::Target> targets;
        unsigned threads = 0;
    };

    Compiler();
    ~Compiler();
    Compiler(const Compiler&) = delete;
    Compiler& operator=(const Compiler&) = delete;

    // Throws `std::invalid_argument` for options that name a variant, a define or a target wrongly or twice, and `CompileError` with every error of the source.
    [[nodiscard]] Package compile(const Options& options) const;

  private:
    struct Job;
    struct Built;

    static void checkOptions(const Options& options);
    [[nodiscard]] static bool isVariantName(std::string_view value) noexcept;
    [[nodiscard]] static std::vector<Job> makeJobs(const ShaderSource& source, const Options& options);
    [[nodiscard]] static std::vector<Package::Target> chooseTargets(const Options& options);
    static void checkTags(const ShaderSource& source, const std::vector<Job>& jobs, const std::vector<Built>& built, std::vector<Diagnostic>& errors);
    [[nodiscard]] static Translation translate(const Built& built, const Job& job, const ShaderSource& source, Package::Target target);
    [[nodiscard]] static Package assemble(const std::filesystem::path& input, std::vector<Built>& built, std::vector<Translation>& translations, std::size_t targets);
};

} // namespace haylen::shaderc
