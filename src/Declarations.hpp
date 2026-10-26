#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ShaderSource.hpp"
#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

// The global declarations of one stage in the order the source declares them, with the places that declare them. The rules of Vulkan that glslang applies give every uniform block, texture and sampler a binding.
struct Declarations {
    // An input or an output of the stage, whose location is the explicit one of the source or the one the compiler assigns.
    struct Variable {
        std::string name;
        std::string type;
        long long id = 0;
        ShaderSource::Location location;
        std::optional<int> explicitLocation;
        int locationSize = 1;
        int assignedLocation = 0;
    };

    // A member of a uniform block with the GLSL type of its elements, which the checks compare with the types the language supports.
    struct Member {
        std::string name;
        std::string type;
        int count = 1;
        ShaderSource::Location location;
    };

    struct Block {
        std::string name;
        int binding = 0;
        ShaderSource::Location location;
        std::vector<Member> members;
    };

    // A texture, whose type is empty when the language does not support it, and the sample type that its declared type gives.
    struct Texture {
        std::string name;
        std::string glslType;
        std::optional<Package::TextureType> type;
        Package::SampleType sampleType = Package::SampleType::Float;
        int binding = 0;
        ShaderSource::Location location;
    };

    struct Sampler {
        std::string name;
        bool shadow = false;
        int binding = 0;
        ShaderSource::Location location;
    };

    Package::Stage stage = Package::Stage::Vertex;
    std::vector<Variable> inputs;
    std::vector<Variable> outputs;
    std::vector<Block> blocks;
    std::vector<Texture> textures;
    std::vector<Sampler> samplers;
};

} // namespace haylen::shaderc
