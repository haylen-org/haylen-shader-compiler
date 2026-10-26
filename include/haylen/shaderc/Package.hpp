#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::shaderc {

// A compiled shader package: the source of every stage for every target, each once, and every program with its reflection and the code of each target, as `docs/format.md` describes.
struct Package {
    enum class Stage { Vertex, Fragment };
    enum class Target { MslMacos, MslIos, MslIosSimulator, Hlsl5, Glsl430, Glsl300es, Wgsl, Spirv };
    enum class TextureType { Texture2D, Texture2DArray, Texture3D, Cube };
    enum class SampleType { Float, UnfilterableFloat, Depth, Sint, Uint };
    enum class SamplerType { Filtering, Nonfiltering, Comparison };

    struct Attribute {
        int location = 0;
        std::string name;
        std::string type;
    };

    struct Member {
        std::string name;
        std::string type;
        int count = 1;
        int offset = 0;
    };

    struct Block {
        std::string name;
        Stage stage = Stage::Vertex;
        int slot = 0;
        int size = 0;
        std::vector<Member> members;
    };

    struct Texture {
        std::string name;
        Stage stage = Stage::Vertex;
        int slot = 0;
        TextureType type = TextureType::Texture2D;
        SampleType sampleType = SampleType::Float;
    };

    struct Sampler {
        std::string name;
        Stage stage = Stage::Vertex;
        int slot = 0;
        SamplerType type = SamplerType::Filtering;
    };

    struct Pair {
        Stage stage = Stage::Vertex;
        int texture = 0;
        int sampler = 0;
        std::string name;
    };

    // A stage of a program for one target: its source in the list of the package, its entry point and, for HLSL, its profile.
    struct Function {
        int source = 0;
        std::string entry;
        std::string profile;
    };

    // Where a target binds a uniform block, texture or sampler: the binding in its descriptor set, which only SPIR-V and WGSL tell apart.
    struct Binding {
        int set = 0;
        int binding = 0;

        friend bool operator==(const Binding&, const Binding&) = default;
    };

    // A uniform block of the GLSL targets, which hold it as one array of vectors.
    struct FlatBlock {
        std::string name;
        std::string type;
        int count = 0;
    };

    // A program for one target, with the binding of every uniform block, texture and sampler slot. The GLSL targets bind their flattened blocks by name instead.
    struct Code {
        Target target = Target::MslMacos;
        Function vertex;
        Function fragment;
        std::map<int, Binding> blocks;
        std::map<int, Binding> textures;
        std::map<int, Binding> samplers;
        std::map<int, FlatBlock> flatBlocks;
    };

    struct Program {
        std::string name;
        std::vector<Attribute> attributes;
        std::vector<Block> blocks;
        std::vector<Texture> textures;
        std::vector<Sampler> samplers;
        std::vector<Pair> pairs;
        int outputs = 0;
        std::vector<Code> targets;
    };

    static constexpr std::string_view kFormat = "haylen-shader";
    static constexpr int kVersion = 2;
    static constexpr int kBlockSlots = 8;
    static constexpr int kTextureSlots = 32;
    static constexpr int kSamplerSlots = 12;
    static constexpr std::array<Target, 8> kTargets = {Target::MslMacos, Target::MslIos, Target::MslIosSimulator, Target::Hlsl5, Target::Glsl430, Target::Glsl300es, Target::Wgsl, Target::Spirv};
    static const std::array<std::string_view, 2> kStageNames;
    static const std::array<std::string_view, 8> kTargetNames;
    static const std::array<std::string_view, 4> kTextureTypeNames;
    static const std::array<std::string_view, 5> kSampleTypeNames;
    static const std::array<std::string_view, 3> kSamplerTypeNames;

    std::string name;
    std::vector<std::string> sources;
    std::vector<Program> programs;

    [[nodiscard]] static std::string_view stageName(Stage stage) noexcept;
    [[nodiscard]] static std::string_view targetName(Target target) noexcept;
    [[nodiscard]] static std::string_view textureTypeName(TextureType type) noexcept;
    [[nodiscard]] static std::string_view sampleTypeName(SampleType type) noexcept;
    [[nodiscard]] static std::string_view samplerTypeName(SamplerType type) noexcept;
    [[nodiscard]] static std::optional<Target> targetFromName(std::string_view value) noexcept;
    [[nodiscard]] static std::optional<SampleType> sampleTypeFromName(std::string_view value) noexcept;
    [[nodiscard]] static std::optional<SamplerType> samplerTypeFromName(std::string_view value) noexcept;
    [[nodiscard]] static bool isGlsl(Target target) noexcept;

  private:
    template <std::size_t Count> [[nodiscard]] static std::optional<std::size_t> find(const std::array<std::string_view, Count>& names, std::string_view value) noexcept {
        for (std::size_t index = 0; index < Count; ++index) {
            if (names[index] == value) {
                return index;
            }
        }
        return std::nullopt;
    }
};

} // namespace haylen::shaderc
