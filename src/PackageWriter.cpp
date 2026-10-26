#include "haylen/shaderc/PackageWriter.hpp"

#include <format>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

#include <nlohmann/json.hpp>

#include "PathText.hpp"

namespace haylen::shaderc {

std::string PackageWriter::write(const Package& package) {
    using Json = nlohmann::ordered_json;

    // clang-format off
    const auto describeFunction = [](const Package::Function& function) {
        Json value = {{"source", function.source}, {"entry", function.entry}};
        if (!function.profile.empty()) {
            value["profile"] = function.profile;
        }
        return value;
    };
    const auto describeBindings = [](const std::map<int, Package::Binding>& bindings, bool sets) {
        Json value = Json::object();
        for (const auto& [slot, binding] : bindings) {
            value[std::to_string(slot)] = sets ? Json{{"set", binding.set}, {"binding", binding.binding}} : Json(binding.binding);
        }
        return value;
    };
    // clang-format on

    Json programs = Json::object();
    for (const Package::Program& program : package.programs) {
        Json attributes = Json::array();
        for (const Package::Attribute& attribute : program.attributes) {
            attributes.push_back({{"location", attribute.location}, {"name", attribute.name}, {"type", attribute.type}});
        }
        Json blocks = Json::array();
        for (const Package::Block& block : program.blocks) {
            Json members = Json::array();
            for (const Package::Member& member : block.members) {
                members.push_back({{"name", member.name}, {"type", member.type}, {"count", member.count}, {"offset", member.offset}});
            }
            blocks.push_back({{"name", block.name}, {"stage", Package::stageName(block.stage)}, {"slot", block.slot}, {"size", block.size}, {"members", std::move(members)}});
        }
        Json textures = Json::array();
        for (const Package::Texture& texture : program.textures) {
            textures.push_back({{"name", texture.name}, {"stage", Package::stageName(texture.stage)}, {"slot", texture.slot}, {"type", Package::textureTypeName(texture.type)}, {"sampleType", Package::sampleTypeName(texture.sampleType)}});
        }
        Json samplers = Json::array();
        for (const Package::Sampler& sampler : program.samplers) {
            samplers.push_back({{"name", sampler.name}, {"stage", Package::stageName(sampler.stage)}, {"slot", sampler.slot}, {"type", Package::samplerTypeName(sampler.type)}});
        }
        Json pairs = Json::array();
        for (const Package::Pair& pair : program.pairs) {
            pairs.push_back({{"stage", Package::stageName(pair.stage)}, {"texture", pair.texture}, {"sampler", pair.sampler}, {"name", pair.name}});
        }

        Json targets = Json::object();
        for (const Package::Code& code : program.targets) {
            Json described = {{"vertex", describeFunction(code.vertex)}, {"fragment", describeFunction(code.fragment)}};
            if (Package::isGlsl(code.target)) {
                Json flat = Json::object();
                for (const auto& [slot, block] : code.flatBlocks) {
                    flat[std::to_string(slot)] = {{"name", block.name}, {"type", block.type}, {"count", block.count}};
                }
                described["blocks"] = std::move(flat);
            } else {
                // The SPIR-V target names the descriptor set of every binding, from which a device lays out its sets.
                const bool sets = code.target == Package::Target::Spirv;
                described["blocks"] = describeBindings(code.blocks, sets);
                described["textures"] = describeBindings(code.textures, sets);
                described["samplers"] = describeBindings(code.samplers, sets);
            }
            targets[std::string(Package::targetName(code.target))] = std::move(described);
        }

        programs[program.name] = {
            {"attributes", std::move(attributes)}, {"blocks", std::move(blocks)}, {"textures", std::move(textures)}, {"samplers", std::move(samplers)}, {"pairs", std::move(pairs)}, {"outputs", program.outputs}, {"targets", std::move(targets)},
        };
    }

    const Json document = {{"format", Package::kFormat}, {"version", Package::kVersion}, {"name", package.name}, {"sources", package.sources}, {"programs", std::move(programs)}};
    return document.dump();
}

void PackageWriter::save(const Package& package, const std::filesystem::path& path) {
    const std::string text = write(package);
    std::filesystem::path temporary = path;
    temporary += temporarySuffix();
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.close();
        if (!stream) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error(std::format("The package \"{}\" cannot be written.", PathText::of(path)));
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error(std::format("The package \"{}\" cannot replace the file at its path, and the system reports \"{}\".", PathText::of(path), error.message()));
    }
}

std::string PackageWriter::temporarySuffix() {
    // Two compilers that write the same package at once each write a temporary file of their own.
    std::random_device device;
    const std::uint64_t value = (static_cast<std::uint64_t>(device()) << 32) | device();
    return std::format(".{:016x}.tmp", value);
}

} // namespace haylen::shaderc
