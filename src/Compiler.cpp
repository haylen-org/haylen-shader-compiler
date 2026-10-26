#include "haylen/shaderc/Compiler.hpp"

#include <algorithm>
#include <format>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "GlslTranslator.hpp"
#include "HlslTranslator.hpp"
#include "MetalTranslator.hpp"
#include "Names.hpp"
#include "PathText.hpp"
#include "Preprocessor.hpp"
#include "Reflector.hpp"
#include "ShaderSource.hpp"
#include "SpirvTranslator.hpp"
#include "StageCompiler.hpp"
#include "ThreadPool.hpp"
#include "Translation.hpp"
#include "WgslTranslator.hpp"
#include "haylen/shaderc/CompileError.hpp"

namespace haylen::shaderc {

// One program to compile under the name it takes in the package, with the defines of its variant.
struct Compiler::Job {
    std::string name;
    const ShaderSource::Program* program = nullptr;
    std::vector<std::string> defines;
};

// A compiled job with its reflection, or the errors that stopped it.
struct Compiler::Built {
    StageCompiler::Result compiled;
    Package::Program program;
    std::vector<Diagnostic> errors;
};

Compiler::Compiler() {
    StageCompiler::initializeProcess();
    WgslTranslator::initializeProcess();
}

Compiler::~Compiler() {
    StageCompiler::finalizeProcess();
}

Package Compiler::compile(const Options& options) const {
    checkOptions(options);
    const ShaderSource source = Preprocessor(options.includes).run(options.input);
    const std::vector<Job> jobs = makeJobs(source, options);
    const std::vector<Package::Target> targets = chooseTargets(options);
    const bool flattenBlocks = std::ranges::any_of(targets, &Package::isGlsl);
    ThreadPool pool(options.threads);

    std::vector<Built> built(jobs.size());
    // clang-format off
    pool.run(jobs.size(), [&](std::size_t index) {
        const Job& job = jobs[index];
        try {
            built[index].compiled = StageCompiler::compile(source, *job.program, job.defines, flattenBlocks);
            built[index].program = Reflector::reflect(job.name, built[index].compiled, *job.program, source);
        } catch (const CompileError& error) {
            built[index].errors = error.getDiagnostics();
        }
    });
    // clang-format on
    std::vector<Diagnostic> errors;
    for (const Built& entry : built) {
        errors.insert(errors.end(), entry.errors.begin(), entry.errors.end());
    }
    if (errors.empty()) {
        checkTags(source, jobs, built, errors);
    }
    if (!errors.empty()) {
        throw CompileError(std::move(errors));
    }

    // Every target translates every job, and each translation keeps its own place, so the package never depends on the order the threads finish in.
    std::vector<Translation> translations(jobs.size() * targets.size());
    std::vector<std::vector<Diagnostic>> failures(translations.size());
    // clang-format off
    pool.run(translations.size(), [&](std::size_t index) {
        const std::size_t job = index / targets.size();
        try {
            translations[index] = translate(built[job], jobs[job], source, targets[index % targets.size()]);
        } catch (const CompileError& error) {
            failures[index] = error.getDiagnostics();
        }
    });
    // clang-format on
    for (const std::vector<Diagnostic>& failure : failures) {
        errors.insert(errors.end(), failure.begin(), failure.end());
    }
    if (!errors.empty()) {
        throw CompileError(std::move(errors));
    }
    return assemble(options.input, built, translations, targets.size());
}

void Compiler::checkOptions(const Options& options) {
    std::set<std::string_view> variants;
    for (const Variant& variant : options.variants) {
        if (!isVariantName(variant.name)) {
            throw std::invalid_argument(std::format("The variant name \"{}\" is not made of letters, digits, underscores and dashes.", variant.name));
        }
        if (!variants.insert(variant.name).second) {
            throw std::invalid_argument(std::format("The variant \"{}\" is given twice.", variant.name));
        }
        for (const std::string& define : variant.defines) {
            if (!Names::isIdentifier(define)) {
                throw std::invalid_argument(std::format("The define \"{}\" of the variant \"{}\" is not an identifier.", define, variant.name));
            }
        }
    }

    std::set<Package::Target> targets;
    for (const Package::Target target : options.targets) {
        if (!targets.insert(target).second) {
            throw std::invalid_argument(std::format("The target \"{}\" is given twice.", Package::targetName(target)));
        }
    }
}

bool Compiler::isVariantName(std::string_view value) noexcept {
    const auto allowed = [](char character) { return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '_' || character == '-'; };
    return !value.empty() && std::ranges::all_of(value, allowed);
}

std::vector<Compiler::Job> Compiler::makeJobs(const ShaderSource& source, const Options& options) {
    if (source.programs.empty()) {
        throw CompileError(Diagnostic{.file = source.files.front(), .message = "The file declares no program. Declare one with \"@program\", such as \"@program sprite sprite_vs sprite_fs\"."});
    }

    std::vector<Job> jobs;
    if (options.variants.empty()) {
        for (const ShaderSource::Program& program : source.programs) {
            jobs.push_back({.name = program.name, .program = &program});
        }
        return jobs;
    }
    if (source.programs.size() > 1) {
        throw CompileError(source.diagnose(source.programs[1].location, std::format("Variants compile the one program of a file, but this file declares {} programs.", source.programs.size())));
    }
    for (const Variant& variant : options.variants) {
        jobs.push_back({.name = variant.name, .program = &source.programs.front(), .defines = variant.defines});
    }
    return jobs;
}

std::vector<Package::Target> Compiler::chooseTargets(const Options& options) {
    // The package lists the targets in their own order, whichever order the options give them in.
    std::vector<Package::Target> targets;
    for (const Package::Target target : Package::kTargets) {
        if (options.targets.empty() || std::ranges::find(options.targets, target) != options.targets.end()) {
            targets.push_back(target);
        }
    }
    return targets;
}

void Compiler::checkTags(const ShaderSource& source, const std::vector<Job>& jobs, const std::vector<Built>& built, std::vector<Diagnostic>& errors) {
    // A tag holds once any variant of any program that compiles its stage declares the texture or sampler it names.
    std::vector<bool> compiled(source.stages.size());
    std::vector<std::set<std::string>> textures(source.stages.size());
    std::vector<std::set<std::string>> samplers(source.stages.size());
    for (std::size_t index = 0; index < jobs.size(); ++index) {
        const ShaderSource::Program& program = *jobs[index].program;
        for (const auto& [stage, declarations] : {std::pair(program.vertex, &built[index].compiled.vertexDeclarations), std::pair(program.fragment, &built[index].compiled.fragmentDeclarations)}) {
            compiled[stage] = true;
            for (const Declarations::Texture& texture : declarations->textures) {
                textures[stage].insert(texture.name);
            }
            for (const Declarations::Sampler& sampler : declarations->samplers) {
                samplers[stage].insert(sampler.name);
            }
        }
    }

    for (std::size_t index = 0; index < source.stages.size(); ++index) {
        if (!compiled[index]) {
            continue;
        }
        const ShaderSource::Stage& stage = source.stages[index];
        for (const ShaderSource::SampleTag& tag : stage.sampleTags) {
            if (!textures[index].contains(tag.texture)) {
                errors.push_back(source.diagnose(tag.location, std::format("The {} stage \"{}\" declares no texture named \"{}\".", Package::stageName(stage.kind), stage.name, tag.texture)));
            }
        }
        for (const ShaderSource::SamplerTag& tag : stage.samplerTags) {
            if (!samplers[index].contains(tag.sampler)) {
                errors.push_back(source.diagnose(tag.location, std::format("The {} stage \"{}\" declares no sampler named \"{}\".", Package::stageName(stage.kind), stage.name, tag.sampler)));
            }
        }
    }
}

Translation Compiler::translate(const Built& built, const Job& job, const ShaderSource& source, Package::Target target) {
    switch (target) {
    case Package::Target::MslMacos:
    case Package::Target::MslIos:
    case Package::Target::MslIosSimulator:
        return MetalTranslator::translate(built.program, built.compiled, source, *job.program, target);
    case Package::Target::Hlsl5:
        return HlslTranslator::translate(built.program, built.compiled, source, *job.program);
    case Package::Target::Glsl430:
    case Package::Target::Glsl300es:
        return GlslTranslator::translate(built.program, built.compiled, source, *job.program, target);
    case Package::Target::Wgsl:
        return WgslTranslator::translate(built.program, built.compiled, source, *job.program);
    case Package::Target::Spirv:
        break;
    }
    return SpirvTranslator::translate(built.program, built.compiled);
}

Package Compiler::assemble(const std::filesystem::path& input, std::vector<Built>& built, std::vector<Translation>& translations, std::size_t targets) {
    // The package lists every distinct stage source once, in the order the programs and their targets first use it.
    Package package{.name = PathText::of(input.stem())};
    std::unordered_map<std::string, int> indices;
    // clang-format off
    const auto intern = [&package, &indices](std::string& text) {
        const auto [found, added] = indices.try_emplace(text, static_cast<int>(package.sources.size()));
        if (added) {
            package.sources.push_back(std::move(text));
        }
        return found->second;
    };
    // clang-format on

    for (std::size_t job = 0; job < built.size(); ++job) {
        Package::Program program = std::move(built[job].program);
        for (std::size_t target = 0; target < targets; ++target) {
            Translation& translation = translations[job * targets + target];
            translation.code.vertex.source = intern(translation.vertexSource);
            translation.code.fragment.source = intern(translation.fragmentSource);
            program.targets.push_back(std::move(translation.code));
        }
        package.programs.push_back(std::move(program));
    }
    return package;
}

} // namespace haylen::shaderc
