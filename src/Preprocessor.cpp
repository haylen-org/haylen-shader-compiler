#include "Preprocessor.hpp"

#include <algorithm>
#include <format>
#include <system_error>
#include <utility>

#include "Names.hpp"
#include "PathText.hpp"
#include "haylen/shaderc/CompileError.hpp"

namespace haylen::shaderc {

const std::array<std::string_view, 9> Preprocessor::kTags = {"@vs", "@fs", "@block", "@end", "@include", "@include_block", "@program", "@sample_type", "@sampler_type"};

Preprocessor::Preprocessor(std::vector<std::filesystem::path> folders) : includes(std::move(folders)) {}

ShaderSource Preprocessor::run(const std::filesystem::path& input) {
    source = {};
    sections.clear();
    programTags.clear();
    visited.clear();
    diagnostics.clear();

    const std::optional<SourceFile> contents = SourceFile::read(input);
    if (!contents) {
        throw CompileError(Diagnostic{.file = PathText::of(input), .message = "The file cannot be read."});
    }
    addFile(input, *contents);
    expandStages();
    resolvePrograms();

    if (!diagnostics.empty()) {
        throw CompileError(std::move(diagnostics));
    }
    return std::move(source);
}

void Preprocessor::addFile(const std::filesystem::path& path, const SourceFile& contents) {
    visited.insert(identity(path));
    const auto file = static_cast<int>(source.files.size());
    source.files.push_back(PathText::of(path.lexically_normal()));
    parseFile(file, path, contents);
}

void Preprocessor::parseFile(int file, const std::filesystem::path& path, const SourceFile& contents) {
    std::optional<std::size_t> open;
    bool strayReported = false;
    for (const SourceFile::Line& line : contents.lines) {
        if (line.tag.empty()) {
            if (open) {
                sections[*open].items.push_back({.line = {.file = file, .number = line.number, .text = line.text}});
            } else if (line.codeColumn != 0 && !strayReported) {
                error({file, line.number, line.codeColumn}, "Code outside a stage or a block is never compiled. Move it into an \"@vs\", \"@fs\" or \"@block\" section.");
                strayReported = true;
            }
            continue;
        }

        const std::string& tag = line.tag.front().text;
        const ShaderSource::Location location{file, line.number, line.tag.front().column};
        const bool opens = tag == "@vs" || tag == "@fs" || tag == "@block";
        const bool outside = tag == "@include" || tag == "@program";
        const bool inside = tag == "@include_block" || tag == "@sample_type" || tag == "@sampler_type";
        if (std::ranges::find(kTags, tag) == kTags.end()) {
            error(location, std::format("The tag \"{}\" does not exist. The tags are {}.", tag, Names::quoted(kTags, "and")));
            continue;
        }
        if (outside && open) {
            error(location, std::format("The tag \"{}\" is only allowed outside stages and blocks.", tag));
            continue;
        }
        if (inside && !open) {
            error(location, std::format("The tag \"{}\" is only allowed inside a stage or a block.", tag));
            continue;
        }

        if (opens) {
            if (open) {
                error(location, std::format("The {} has no \"@end\" before this tag.", describe(sections[*open])));
            }
            open = openSection(file, line, tag == "@vs" ? SectionKind::Vertex : tag == "@fs" ? SectionKind::Fragment : SectionKind::Block);
            strayReported = false;
        } else if (tag == "@end") {
            if (!open) {
                error(location, "The tag \"@end\" has no stage or block to close.");
                continue;
            }
            checkArguments(file, line, 0);
            open.reset();
        } else if (tag == "@include") {
            includeFile(file, path, line);
        } else if (tag == "@program") {
            addProgram(file, line);
        } else if (tag == "@include_block") {
            if (checkArguments(file, line, 1) && checkName(file, line, line.tag[1])) {
                sections[*open].items.push_back({.block = line.tag[1].text, .location = {file, line.number, line.tag[1].column}});
            }
        } else if (tag == "@sample_type") {
            addSampleTag(file, line, *open);
        } else {
            addSamplerTag(file, line, *open);
        }
    }

    if (open) {
        error(sections[*open].location, std::format("The {} has no \"@end\".", describe(sections[*open])));
    }
}

std::size_t Preprocessor::openSection(int file, const SourceFile::Line& line, SectionKind kind) {
    Section section{.kind = kind, .location = {file, line.number, line.tag.front().column}};
    if (checkArguments(file, line, 1) && checkName(file, line, line.tag[1])) {
        const std::string& name = line.tag[1].text;
        const std::optional<std::size_t> existing = findSection(name, kind == SectionKind::Block);
        if (existing) {
            const Section& taken = sections[*existing];
            error({file, line.number, line.tag[1].column}, std::format("The name \"{}\" is already the {} declared {}.", name, kindName(taken.kind), describe(taken.location)));
        } else {
            section.name = name;
        }
    }
    sections.push_back(std::move(section));
    return sections.size() - 1;
}

void Preprocessor::includeFile(int file, const std::filesystem::path& from, const SourceFile::Line& line) {
    if (line.tag.size() < 2) {
        error({file, line.number, line.tag.front().column}, std::string(usageOf("@include")));
        return;
    }

    // The path is the rest of the line, so it may hold spaces.
    const SourceFile::Word& first = line.tag[1];
    const SourceFile::Word& last = line.tag.back();
    const auto start = static_cast<std::size_t>(first.column - 1);
    const std::string text = line.text.substr(start, static_cast<std::size_t>(last.column - 1) + last.text.size() - start);
    const ShaderSource::Location location{file, line.number, first.column};

    const std::optional<std::filesystem::path> found = resolve(from, PathText::from(text));
    if (!found) {
        error(location, std::format("The file \"{}\" is neither next to this file nor in an include folder.", text));
        return;
    }
    if (visited.contains(identity(*found))) {
        return;
    }
    const std::optional<SourceFile> contents = SourceFile::read(*found);
    if (!contents) {
        error(location, std::format("The file \"{}\" cannot be read.", PathText::of(*found)));
        return;
    }
    addFile(*found, *contents);
}

void Preprocessor::addProgram(int file, const SourceFile::Line& line) {
    if (!checkArguments(file, line, 3)) {
        return;
    }
    bool named = true;
    for (std::size_t index = 1; index <= 3; ++index) {
        named = checkName(file, line, line.tag[index]) && named;
    }
    if (!named) {
        return;
    }

    const std::string& name = line.tag[1].text;
    const auto existing = std::ranges::find(programTags, name, &ProgramTag::name);
    if (existing != programTags.end()) {
        error({file, line.number, line.tag[1].column}, std::format("The program \"{}\" is already declared {}.", name, describe(existing->location)));
        return;
    }
    programTags.push_back({
        .name = name,
        .location = {file, line.number, line.tag.front().column},
        .vertex = line.tag[2].text,
        .vertexLocation = {file, line.number, line.tag[2].column},
        .fragment = line.tag[3].text,
        .fragmentLocation = {file, line.number, line.tag[3].column},
    });
}

void Preprocessor::addSampleTag(int file, const SourceFile::Line& line, std::size_t section) {
    if (!checkArguments(file, line, 2) || !checkName(file, line, line.tag[1])) {
        return;
    }
    const std::optional<Package::SampleType> type = Package::sampleTypeFromName(line.tag[2].text);
    if (!type) {
        error({file, line.number, line.tag[2].column}, std::format("The sample type \"{}\" is not {}.", line.tag[2].text, Names::quoted(Package::kSampleTypeNames, "or")));
        return;
    }
    sections[section].sampleTags.push_back({.texture = line.tag[1].text, .type = *type, .location = {file, line.number, line.tag[1].column}});
}

void Preprocessor::addSamplerTag(int file, const SourceFile::Line& line, std::size_t section) {
    if (!checkArguments(file, line, 2) || !checkName(file, line, line.tag[1])) {
        return;
    }
    const std::optional<Package::SamplerType> type = Package::samplerTypeFromName(line.tag[2].text);
    if (!type) {
        error({file, line.number, line.tag[2].column}, std::format("The sampler type \"{}\" is not {}.", line.tag[2].text, Names::quoted(Package::kSamplerTypeNames, "or")));
        return;
    }
    sections[section].samplerTags.push_back({.sampler = line.tag[1].text, .type = *type, .location = {file, line.number, line.tag[1].column}});
}

void Preprocessor::expandStages() {
    for (std::size_t index = 0; index < sections.size(); ++index) {
        const Section& section = sections[index];
        if (section.kind == SectionKind::Block || section.name.empty()) {
            continue;
        }

        ShaderSource::Stage stage{.name = section.name, .kind = section.kind == SectionKind::Vertex ? Package::Stage::Vertex : Package::Stage::Fragment, .location = section.location};
        std::vector<std::string> chain;
        expand(index, stage, chain);
        checkTags(stage);
        source.stages.push_back(std::move(stage));
    }
}

void Preprocessor::expand(std::size_t index, ShaderSource::Stage& stage, std::vector<std::string>& chain) {
    const Section& section = sections[index];
    stage.sampleTags.insert(stage.sampleTags.end(), section.sampleTags.begin(), section.sampleTags.end());
    stage.samplerTags.insert(stage.samplerTags.end(), section.samplerTags.begin(), section.samplerTags.end());
    for (const Item& item : section.items) {
        if (item.block.empty()) {
            stage.lines.push_back(item.line);
            continue;
        }

        const std::optional<std::size_t> block = findSection(item.block, true);
        if (!block) {
            error(item.location, std::format("There is no block named \"{}\".", item.block));
            continue;
        }
        if (std::ranges::find(chain, item.block) != chain.end()) {
            error(item.location, std::format("The block \"{}\" includes itself.", item.block));
            continue;
        }
        chain.push_back(item.block);
        expand(*block, stage, chain);
        chain.pop_back();
    }
}

void Preprocessor::checkTags(const ShaderSource::Stage& stage) {
    for (std::size_t index = 0; index < stage.sampleTags.size(); ++index) {
        const ShaderSource::SampleTag& tag = stage.sampleTags[index];
        const auto earlier = std::find_if(stage.sampleTags.begin(), stage.sampleTags.begin() + static_cast<std::ptrdiff_t>(index), [&tag](const ShaderSource::SampleTag& other) { return other.texture == tag.texture && other.type != tag.type; });
        if (earlier != stage.sampleTags.begin() + static_cast<std::ptrdiff_t>(index)) {
            error(tag.location, std::format("The texture \"{}\" already has the sample type \"{}\" {}.", tag.texture, Package::sampleTypeName(earlier->type), describe(earlier->location)));
        }
    }
    for (std::size_t index = 0; index < stage.samplerTags.size(); ++index) {
        const ShaderSource::SamplerTag& tag = stage.samplerTags[index];
        const auto earlier = std::find_if(stage.samplerTags.begin(), stage.samplerTags.begin() + static_cast<std::ptrdiff_t>(index), [&tag](const ShaderSource::SamplerTag& other) { return other.sampler == tag.sampler && other.type != tag.type; });
        if (earlier != stage.samplerTags.begin() + static_cast<std::ptrdiff_t>(index)) {
            error(tag.location, std::format("The sampler \"{}\" already has the type \"{}\" {}.", tag.sampler, Package::samplerTypeName(earlier->type), describe(earlier->location)));
        }
    }
}

void Preprocessor::resolvePrograms() {
    for (const ProgramTag& tag : programTags) {
        const std::optional<std::size_t> vertex = resolveStage(tag.vertex, tag.vertexLocation, Package::Stage::Vertex);
        const std::optional<std::size_t> fragment = resolveStage(tag.fragment, tag.fragmentLocation, Package::Stage::Fragment);
        if (vertex && fragment) {
            source.programs.push_back({.name = tag.name, .location = tag.location, .vertex = *vertex, .fragment = *fragment});
        }
    }
}

std::optional<std::size_t> Preprocessor::resolveStage(const std::string& name, const ShaderSource::Location& location, Package::Stage kind) {
    const auto found = std::ranges::find(source.stages, name, &ShaderSource::Stage::name);
    if (found == source.stages.end()) {
        error(location, std::format("There is no stage named \"{}\".", name));
        return std::nullopt;
    }
    if (found->kind != kind) {
        const bool vertex = kind == Package::Stage::Vertex;
        error(location, std::format("The stage \"{}\" is a {} stage, but the {} name of \"@program\" is the {} stage.", name, Package::stageName(found->kind), vertex ? "second" : "third", Package::stageName(kind)));
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - source.stages.begin());
}

bool Preprocessor::checkArguments(int file, const SourceFile::Line& line, std::size_t count) {
    const std::size_t given = line.tag.size() - 1;
    if (given == count) {
        return true;
    }
    const SourceFile::Word& word = given < count ? line.tag.front() : line.tag[count + 1];
    error({file, line.number, word.column}, std::string(usageOf(line.tag.front().text)));
    return false;
}

bool Preprocessor::checkName(int file, const SourceFile::Line& line, const SourceFile::Word& word) {
    if (Names::isIdentifier(word.text)) {
        return true;
    }
    error({file, line.number, word.column}, std::format("The name \"{}\" is not an identifier. A name starts with a letter or an underscore and goes on with letters, digits and underscores.", word.text));
    return false;
}

std::optional<std::filesystem::path> Preprocessor::resolve(const std::filesystem::path& from, const std::filesystem::path& path) const {
    std::vector<std::filesystem::path> candidates;
    if (path.is_absolute()) {
        candidates.push_back(path);
    } else {
        candidates.push_back(from.parent_path() / path);
        for (const std::filesystem::path& folder : includes) {
            candidates.push_back(folder / path);
        }
    }

    for (const std::filesystem::path& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate.lexically_normal();
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> Preprocessor::findSection(std::string_view name, bool block) const {
    for (std::size_t index = 0; index < sections.size(); ++index) {
        const Section& section = sections[index];
        if (!section.name.empty() && section.name == name && (section.kind == SectionKind::Block) == block) {
            return index;
        }
    }
    return std::nullopt;
}

std::string Preprocessor::describe(const Section& section) const {
    if (section.name.empty()) {
        return std::format("{} that starts {}", kindName(section.kind), describe(section.location));
    }
    return std::format("{} \"{}\"", kindName(section.kind), section.name);
}

std::string Preprocessor::describe(const ShaderSource::Location& location) const {
    return std::format("at line {} of \"{}\"", location.line, source.files[static_cast<std::size_t>(location.file)]);
}

void Preprocessor::error(const ShaderSource::Location& location, std::string message) {
    diagnostics.push_back(source.diagnose(location, std::move(message)));
}

std::filesystem::path Preprocessor::identity(const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    return error ? path.lexically_normal() : canonical;
}

std::string_view Preprocessor::kindName(SectionKind kind) noexcept {
    switch (kind) {
    case SectionKind::Vertex:
        return "vertex stage";
    case SectionKind::Fragment:
        return "fragment stage";
    case SectionKind::Block:
        break;
    }
    return "block";
}

std::string_view Preprocessor::usageOf(std::string_view tag) noexcept {
    if (tag == "@vs") {
        return "The tag \"@vs\" takes the name of the vertex stage, such as \"@vs sprite_vs\".";
    }
    if (tag == "@fs") {
        return "The tag \"@fs\" takes the name of the fragment stage, such as \"@fs sprite_fs\".";
    }
    if (tag == "@block") {
        return "The tag \"@block\" takes the name of the block, such as \"@block lighting\".";
    }
    if (tag == "@end") {
        return "The tag \"@end\" takes no arguments.";
    }
    if (tag == "@include") {
        return "The tag \"@include\" takes the path of a file, such as \"@include haylen/material.glsl\".";
    }
    if (tag == "@include_block") {
        return "The tag \"@include_block\" takes the name of a block, such as \"@include_block lighting\".";
    }
    if (tag == "@program") {
        return "The tag \"@program\" takes the name of the program, its vertex stage and its fragment stage, such as \"@program sprite sprite_vs sprite_fs\".";
    }
    if (tag == "@sample_type") {
        return "The tag \"@sample_type\" takes a texture and its sample type, such as \"@sample_type shadow_texture unfilterable_float\".";
    }
    return "The tag \"@sampler_type\" takes a sampler and its type, such as \"@sampler_type shadow_sampler nonfiltering\".";
}

} // namespace haylen::shaderc
