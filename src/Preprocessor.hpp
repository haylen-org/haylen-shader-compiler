#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "ShaderSource.hpp"
#include "SourceFile.hpp"
#include "haylen/shaderc/Diagnostic.hpp"

namespace haylen::shaderc {

// Reads the tags of a source and of the files it includes into its stages and programs.
class Preprocessor {
  public:
    explicit Preprocessor(std::vector<std::filesystem::path> folders);

    // Throws `CompileError` with every error of the tags.
    [[nodiscard]] ShaderSource run(const std::filesystem::path& input);

  private:
    enum class SectionKind { Vertex, Fragment, Block };

    // A line of code of a section, or the block that an "@include_block" tag names.
    struct Item {
        ShaderSource::Line line;
        std::string block;
        ShaderSource::Location location;
    };

    // A stage or a block as its file writes it. A section whose name is wrong or taken has no name and never compiles.
    struct Section {
        std::string name;
        SectionKind kind = SectionKind::Block;
        ShaderSource::Location location;
        std::vector<Item> items;
        std::vector<ShaderSource::SampleTag> sampleTags;
        std::vector<ShaderSource::SamplerTag> samplerTags;
    };

    struct ProgramTag {
        std::string name;
        ShaderSource::Location location;
        std::string vertex;
        ShaderSource::Location vertexLocation;
        std::string fragment;
        ShaderSource::Location fragmentLocation;
    };

    static const std::array<std::string_view, 9> kTags;

    std::vector<std::filesystem::path> includes;
    ShaderSource source;
    std::vector<Section> sections;
    std::vector<ProgramTag> programTags;
    std::set<std::filesystem::path> visited;
    std::vector<Diagnostic> diagnostics;

    void addFile(const std::filesystem::path& path, const SourceFile& contents);
    void parseFile(int file, const std::filesystem::path& path, const SourceFile& contents);
    [[nodiscard]] std::size_t openSection(int file, const SourceFile::Line& line, SectionKind kind);
    void includeFile(int file, const std::filesystem::path& from, const SourceFile::Line& line);
    void addProgram(int file, const SourceFile::Line& line);
    void addSampleTag(int file, const SourceFile::Line& line, std::size_t section);
    void addSamplerTag(int file, const SourceFile::Line& line, std::size_t section);
    void expandStages();
    void expand(std::size_t index, ShaderSource::Stage& stage, std::vector<std::string>& chain);
    void checkTags(const ShaderSource::Stage& stage);
    void resolvePrograms();
    [[nodiscard]] std::optional<std::size_t> resolveStage(const std::string& name, const ShaderSource::Location& location, Package::Stage kind);

    bool checkArguments(int file, const SourceFile::Line& line, std::size_t count);
    bool checkName(int file, const SourceFile::Line& line, const SourceFile::Word& word);
    [[nodiscard]] std::optional<std::filesystem::path> resolve(const std::filesystem::path& from, const std::filesystem::path& path) const;
    [[nodiscard]] std::optional<std::size_t> findSection(std::string_view name, bool block) const;
    [[nodiscard]] std::string describe(const Section& section) const;
    [[nodiscard]] std::string describe(const ShaderSource::Location& location) const;
    void error(const ShaderSource::Location& location, std::string message);

    [[nodiscard]] static std::filesystem::path identity(const std::filesystem::path& path);
    [[nodiscard]] static std::string_view kindName(SectionKind kind) noexcept;
    [[nodiscard]] static std::string_view usageOf(std::string_view tag) noexcept;
};

} // namespace haylen::shaderc
