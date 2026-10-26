#include "DeclarationChecker.hpp"

#include <algorithm>
#include <format>

#include "Names.hpp"

namespace haylen::shaderc {

const std::array<std::string_view, 9> DeclarationChecker::kMemberTypes = {"float", "vec2", "vec3", "vec4", "int", "ivec2", "ivec3", "ivec4", "mat4"};
const std::array<std::string_view, 4> DeclarationChecker::kTextureTypes = {"texture2D", "texture2DArray", "texture3D", "textureCube"};

void DeclarationChecker::check(const Declarations& vertex, const Declarations& fragment, const ShaderSource::Program& program, bool flattenBlocks, const ShaderSource& source, std::vector<Diagnostic>& diagnostics) {
    Slots slots;
    checkStage(vertex, source.stages[program.vertex], flattenBlocks, source, slots, diagnostics);
    checkStage(fragment, source.stages[program.fragment], flattenBlocks, source, slots, diagnostics);

    // Every slot of a kind belongs to one declaration of the program, whichever stage declares it.
    checkUnique("uniform block", slots.blocks, source, diagnostics);
    checkUnique("texture", slots.textures, source, diagnostics);
    checkUnique("sampler", slots.samplers, source, diagnostics);
}

void DeclarationChecker::checkStage(const Declarations& declarations, const ShaderSource::Stage& stage, bool flattenBlocks, const ShaderSource& source, Slots& slots, std::vector<Diagnostic>& diagnostics) {
    for (const Declarations::Block& block : declarations.blocks) {
        if (checkBinding("uniform block", block.name, block.binding, Package::kBlockSlots, block.location, source, diagnostics)) {
            slots.blocks.push_back({.slot = block.binding, .name = block.name, .location = block.location});
        }
        checkBlock(block, flattenBlocks, source, diagnostics);
    }
    for (const Declarations::Texture& texture : declarations.textures) {
        if (checkBinding("texture", texture.name, texture.binding, Package::kTextureSlots, texture.location, source, diagnostics)) {
            slots.textures.push_back({.slot = texture.binding, .name = texture.name, .location = texture.location});
        }
        if (!texture.type) {
            diagnostics.push_back(source.diagnose(texture.location, std::format("The texture \"{}\" is a \"{}\", but textures are {}, with \"i\" or \"u\" in front for integers.", texture.name, texture.glslType, Names::quoted(kTextureTypes, "or"))));
        }
    }
    for (const Declarations::Sampler& sampler : declarations.samplers) {
        if (checkBinding("sampler", sampler.name, sampler.binding, Package::kSamplerSlots, sampler.location, source, diagnostics)) {
            slots.samplers.push_back({.slot = sampler.binding, .name = sampler.name, .location = sampler.location});
        }
    }
    checkTags(declarations, stage, source, diagnostics);
}

void DeclarationChecker::checkBlock(const Declarations::Block& block, bool flattenBlocks, const ShaderSource& source, std::vector<Diagnostic>& diagnostics) {
    bool floats = false;
    bool integers = false;
    for (const Declarations::Member& member : block.members) {
        if (std::ranges::find(kMemberTypes, member.type) == kMemberTypes.end()) {
            diagnostics.push_back(source.diagnose(member.location, std::format("The member \"{}\" of the uniform block \"{}\" is a \"{}\", but members are {}.", member.name, block.name, member.type, Names::quoted(kMemberTypes, "or"))));
            continue;
        }
        (member.type.starts_with('i') ? integers : floats) = true;
    }

    // The GLSL targets hold a block as one array of vectors of a single kind of number.
    if (flattenBlocks && floats && integers) {
        diagnostics.push_back(source.diagnose(block.location, std::format("The uniform block \"{}\" holds both floats and integers, which the GLSL targets cannot flatten into one array. Split it into a block of floats and a block of integers.", block.name)));
    }
}

void DeclarationChecker::checkTags(const Declarations& declarations, const ShaderSource::Stage& stage, const ShaderSource& source, std::vector<Diagnostic>& diagnostics) {
    for (const ShaderSource::SampleTag& tag : stage.sampleTags) {
        const auto texture = std::ranges::find(declarations.textures, tag.texture, &Declarations::Texture::name);
        if (texture == declarations.textures.end() || !texture->type) {
            continue;
        }
        if (texture->sampleType == Package::SampleType::Float && (tag.type == Package::SampleType::Sint || tag.type == Package::SampleType::Uint)) {
            diagnostics.push_back(source.diagnose(tag.location, std::format("The texture \"{}\" holds floats, so its sample type is \"float\", \"unfilterable_float\" or \"depth\", not \"{}\".", tag.texture, Package::sampleTypeName(tag.type))));
        } else if (texture->sampleType != Package::SampleType::Float && tag.type != texture->sampleType) {
            const bool signedType = texture->sampleType == Package::SampleType::Sint;
            diagnostics.push_back(source.diagnose(tag.location, std::format("The texture \"{}\" holds {} integers, so its sample type is \"{}\", not \"{}\".", tag.texture, signedType ? "signed" : "unsigned", Package::sampleTypeName(texture->sampleType), Package::sampleTypeName(tag.type))));
        }
    }

    for (const ShaderSource::SamplerTag& tag : stage.samplerTags) {
        const auto sampler = std::ranges::find(declarations.samplers, tag.sampler, &Declarations::Sampler::name);
        if (sampler == declarations.samplers.end()) {
            continue;
        }
        if (sampler->shadow && tag.type != Package::SamplerType::Comparison) {
            diagnostics.push_back(source.diagnose(tag.location, std::format("The sampler \"{}\" is a \"samplerShadow\", so its type is \"comparison\", not \"{}\".", tag.sampler, Package::samplerTypeName(tag.type))));
        } else if (!sampler->shadow && tag.type == Package::SamplerType::Comparison) {
            diagnostics.push_back(source.diagnose(tag.location, std::format("The sampler \"{}\" is a \"sampler\", so its type is \"filtering\" or \"nonfiltering\", not \"comparison\". Declare it as a \"samplerShadow\" to compare depths.", tag.sampler)));
        }
    }
}

bool DeclarationChecker::checkBinding(std::string_view kind, const std::string& name, int binding, int slots, const ShaderSource::Location& location, const ShaderSource& source, std::vector<Diagnostic>& diagnostics) {
    if (binding < slots) {
        return true;
    }
    diagnostics.push_back(source.diagnose(location, std::format("The {} \"{}\" binds to {}, but {}s bind from 0 to {}.", kind, name, binding, kind, slots - 1)));
    return false;
}

void DeclarationChecker::checkUnique(std::string_view kind, const std::vector<Binding>& bindings, const ShaderSource& source, std::vector<Diagnostic>& diagnostics) {
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        const Binding& binding = bindings[index];
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            const Binding& taken = bindings[earlier];
            if (taken.slot == binding.slot) {
                const std::string& file = source.files[static_cast<std::size_t>(taken.location.file)];
                diagnostics.push_back(source.diagnose(binding.location, std::format("The {} \"{}\" binds to {}, which the {} \"{}\" at line {} of \"{}\" already takes.", kind, binding.name, binding.slot, kind, taken.name, taken.location.line, file)));
                break;
            }
        }
    }
}

} // namespace haylen::shaderc
