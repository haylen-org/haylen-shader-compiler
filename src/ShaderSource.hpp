#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "haylen/shaderc/Diagnostic.hpp"
#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

// A source after its tags: the code of every stage with the blocks it includes pasted in, where every line keeps the file and the line it comes from, and the programs that pair the stages.
struct ShaderSource {
    // A place in one of the files of the source, which a file index names.
    struct Location {
        int file = 0;
        int line = 0;
        int column = 0;
    };

    struct Line {
        int file = 0;
        int number = 0;
        std::string text;
    };

    struct SampleTag {
        std::string texture;
        Package::SampleType type = Package::SampleType::Float;
        Location location;
    };

    struct SamplerTag {
        std::string sampler;
        Package::SamplerType type = Package::SamplerType::Filtering;
        Location location;
    };

    // A stage with the tags written in it and in the blocks it includes.
    struct Stage {
        std::string name;
        Package::Stage kind = Package::Stage::Vertex;
        Location location;
        std::vector<Line> lines;
        std::vector<SampleTag> sampleTags;
        std::vector<SamplerTag> samplerTags;
    };

    struct Program {
        std::string name;
        Location location;
        std::size_t vertex = 0;
        std::size_t fragment = 0;
    };

    std::vector<std::string> files;
    std::vector<Stage> stages;
    std::vector<Program> programs;

    [[nodiscard]] Diagnostic diagnose(const Location& location, std::string message) const;
};

} // namespace haylen::shaderc
