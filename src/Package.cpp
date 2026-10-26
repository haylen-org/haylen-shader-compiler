#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

const std::array<std::string_view, 2> Package::kStageNames = {"vertex", "fragment"};
const std::array<std::string_view, 8> Package::kTargetNames = {"msl-macos", "msl-ios", "msl-ios-simulator", "hlsl5", "glsl430", "glsl300es", "wgsl", "spirv"};
const std::array<std::string_view, 4> Package::kTextureTypeNames = {"2d", "2d_array", "3d", "cube"};
const std::array<std::string_view, 5> Package::kSampleTypeNames = {"float", "unfilterable_float", "depth", "sint", "uint"};
const std::array<std::string_view, 3> Package::kSamplerTypeNames = {"filtering", "nonfiltering", "comparison"};

std::string_view Package::stageName(Stage stage) noexcept {
    return kStageNames[static_cast<std::size_t>(stage)];
}

std::string_view Package::targetName(Target target) noexcept {
    return kTargetNames[static_cast<std::size_t>(target)];
}

std::string_view Package::textureTypeName(TextureType type) noexcept {
    return kTextureTypeNames[static_cast<std::size_t>(type)];
}

std::string_view Package::sampleTypeName(SampleType type) noexcept {
    return kSampleTypeNames[static_cast<std::size_t>(type)];
}

std::string_view Package::samplerTypeName(SamplerType type) noexcept {
    return kSamplerTypeNames[static_cast<std::size_t>(type)];
}

std::optional<Package::Target> Package::targetFromName(std::string_view value) noexcept {
    const std::optional<std::size_t> index = find(kTargetNames, value);
    return index ? std::optional(static_cast<Target>(*index)) : std::nullopt;
}

std::optional<Package::SampleType> Package::sampleTypeFromName(std::string_view value) noexcept {
    const std::optional<std::size_t> index = find(kSampleTypeNames, value);
    return index ? std::optional(static_cast<SampleType>(*index)) : std::nullopt;
}

std::optional<Package::SamplerType> Package::samplerTypeFromName(std::string_view value) noexcept {
    const std::optional<std::size_t> index = find(kSamplerTypeNames, value);
    return index ? std::optional(static_cast<SamplerType>(*index)) : std::nullopt;
}

bool Package::isGlsl(Target target) noexcept {
    return target == Target::Glsl430 || target == Target::Glsl300es;
}

} // namespace haylen::shaderc
