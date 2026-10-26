#include "DeclarationReader.hpp"

#include <format>
#include <map>

#include <glslang/MachineIndependent/localintermediate.h>

namespace haylen::shaderc {

// Finds the place of every declaration in the declaration nodes that a stage parsed with debug information holds, since the linker objects keep none.
class DeclarationReader::PlaceFinder : public glslang::TIntermTraverser {
  public:
    std::map<long long, glslang::TSourceLoc> places;

    bool visitVariableDecl(glslang::TVisit, glslang::TIntermVariableDecl* node) override {
        places.emplace(node->getDeclSymbol()->getId(), node->getLoc());
        return true;
    }
};

Declarations DeclarationReader::read(const glslang::TIntermediate& intermediate, const ShaderSource::Stage& stage, const ShaderSource& source, std::vector<Diagnostic>& diagnostics) {
    Declarations declarations{.stage = stage.kind};
    PlaceFinder finder;
    intermediate.getTreeRoot()->traverse(&finder);

    // The linker objects of glslang list every global of the stage in the order the source declares them.
    for (TIntermNode* node : intermediate.findLinkerObjects()->getSequence()) {
        const glslang::TIntermSymbol* symbol = node->getAsSymbolNode();
        if (symbol == nullptr) {
            continue;
        }

        const glslang::TType& type = symbol->getType();
        const glslang::TQualifier& qualifier = type.getQualifier();
        const std::string name = symbol->getName().c_str();
        const auto place = finder.places.find(symbol->getId());
        const ShaderSource::Location location = toLocation(place == finder.places.end() ? symbol->getLoc() : place->second, stage, source);
        switch (qualifier.storage) {
        case glslang::EvqVaryingIn:
        case glslang::EvqVaryingOut: {
            if (qualifier.builtIn != glslang::EbvNone || isBuiltInBlock(type)) {
                break;
            }
            if (type.getBasicType() == glslang::EbtBlock) {
                diagnostics.push_back(source.diagnose(location, std::format("Input and output blocks such as \"{}\" are not supported. Declare each input or output apart.", type.getTypeName().c_str())));
                break;
            }
            Declarations::Variable variable{
                .name = name,
                .type = typeName(type),
                .id = symbol->getId(),
                .location = location,
                .explicitLocation = qualifier.hasLocation() ? std::optional(static_cast<int>(qualifier.layoutLocation)) : std::nullopt,
                .locationSize = glslang::TIntermediate::computeTypeLocationSize(type, intermediate.getStage()),
            };
            (qualifier.storage == glslang::EvqVaryingIn ? declarations.inputs : declarations.outputs).push_back(std::move(variable));
            break;
        }
        case glslang::EvqUniform:
            readUniform(declarations, type, name, location, stage, source, diagnostics);
            break;
        case glslang::EvqBuffer:
            diagnostics.push_back(source.diagnose(location, std::format("Storage buffers such as \"{}\" are not supported.", type.getTypeName().c_str())));
            break;
        default:
            break;
        }
    }
    return declarations;
}

void DeclarationReader::readUniform(Declarations& declarations, const glslang::TType& type, const std::string& name, const ShaderSource::Location& location, const ShaderSource::Stage& stage, const ShaderSource& source, std::vector<Diagnostic>& diagnostics) {
    const glslang::TQualifier& qualifier = type.getQualifier();
    const std::string shown = type.getBasicType() == glslang::EbtBlock ? std::string(type.getTypeName().c_str()) : name;
    if (qualifier.hasSet() && qualifier.layoutSet != 0) {
        diagnostics.push_back(source.diagnose(location, std::format("The language has no descriptor sets, so \"{}\" cannot take \"set\". Remove it from its layout.", shown)));
        return;
    }
    const auto binding = static_cast<int>(qualifier.layoutBinding);

    if (type.getBasicType() == glslang::EbtBlock) {
        if (qualifier.isPushConstant()) {
            diagnostics.push_back(source.diagnose(location, std::format("Push constants such as \"{}\" are not supported. Declare a uniform block with a binding instead.", shown)));
            return;
        }
        Declarations::Block block{.name = shown, .binding = binding, .location = location};
        for (const glslang::TTypeLoc& member : *type.getStruct()) {
            const glslang::TType& memberType = *member.type;
            const bool array = memberType.isArray();
            block.members.push_back({
                .name = memberType.getFieldName().c_str(),
                .type = array && memberType.getArraySizes()->getNumDims() > 1 ? typeName(memberType) : elementName(memberType),
                .count = array ? memberType.getOuterArraySize() : 1,
                .location = toLocation(member.loc, stage, source),
            });
        }
        declarations.blocks.push_back(std::move(block));
        return;
    }

    if (type.getBasicType() != glslang::EbtSampler) {
        return;
    }
    if (type.isArray()) {
        diagnostics.push_back(source.diagnose(location, std::format("Arrays of textures or samplers such as \"{}\" are not supported. Declare each one apart.", name)));
        return;
    }
    const glslang::TSampler& sampler = type.getSampler();
    if (sampler.isPureSampler()) {
        declarations.samplers.push_back({.name = name, .shadow = sampler.shadow, .binding = binding, .location = location});
    } else if (sampler.isCombined()) {
        diagnostics.push_back(source.diagnose(location, std::format("The combined texture sampler \"{}\" is not part of the language. Declare a texture and a sampler apart and combine them where the stage samples, such as \"texture(sampler2D(color_texture, color_sampler), uv)\".", name)));
    } else if (sampler.isImageClass()) {
        diagnostics.push_back(source.diagnose(location, std::format("Storage images such as \"{}\" are not supported.", name)));
    } else {
        const glslang::TBasicType basic = sampler.type;
        const bool known = basic == glslang::EbtFloat || basic == glslang::EbtInt || basic == glslang::EbtUint;
        declarations.textures.push_back({
            .name = name,
            .glslType = sampler.getString(),
            .type = known ? textureType(sampler) : std::nullopt,
            .sampleType = basic == glslang::EbtInt    ? Package::SampleType::Sint
                          : basic == glslang::EbtUint ? Package::SampleType::Uint
                                                      : Package::SampleType::Float,
            .binding = binding,
            .location = location,
        });
    }
}

ShaderSource::Location DeclarationReader::toLocation(const glslang::TSourceLoc& place, const ShaderSource::Stage& stage, const ShaderSource& source) {
    // A "#line" directive of the source itself may name a file that does not exist, and such places keep the file of the stage.
    const bool known = place.string >= 0 && static_cast<std::size_t>(place.string) < source.files.size();
    return {.file = known ? place.string : stage.location.file, .line = place.line, .column = place.column};
}

std::string DeclarationReader::typeName(const glslang::TType& type) {
    std::string name = elementName(type);
    if (type.isArray()) {
        const glslang::TArraySizes& sizes = *type.getArraySizes();
        for (int dimension = 0; dimension < sizes.getNumDims(); ++dimension) {
            name += std::format("[{}]", sizes.getDimSize(dimension));
        }
    }
    return name;
}

std::string DeclarationReader::elementName(const glslang::TType& type) {
    if (type.isStruct()) {
        return type.getTypeName().c_str();
    }

    std::string prefix;
    std::string scalar;
    switch (type.getBasicType()) {
    case glslang::EbtFloat:
        scalar = "float";
        break;
    case glslang::EbtDouble:
        prefix = "d";
        scalar = "double";
        break;
    case glslang::EbtInt:
        prefix = "i";
        scalar = "int";
        break;
    case glslang::EbtUint:
        prefix = "u";
        scalar = "uint";
        break;
    case glslang::EbtBool:
        prefix = "b";
        scalar = "bool";
        break;
    default:
        return type.getBasicTypeString().c_str();
    }

    if (type.isMatrix()) {
        const int columns = type.getMatrixCols();
        const int rows = type.getMatrixRows();
        return columns == rows ? std::format("{}mat{}", prefix, columns) : std::format("{}mat{}x{}", prefix, columns, rows);
    }
    if (type.isVector()) {
        return std::format("{}vec{}", prefix, type.getVectorSize());
    }
    return scalar;
}

std::optional<Package::TextureType> DeclarationReader::textureType(const glslang::TSampler& sampler) {
    if (sampler.isMultiSample() || sampler.isExternal() || sampler.isYuv()) {
        return std::nullopt;
    }
    switch (sampler.dim) {
    case glslang::Esd2D:
        return sampler.isArrayed() ? Package::TextureType::Texture2DArray : Package::TextureType::Texture2D;
    case glslang::Esd3D:
        return sampler.isArrayed() ? std::nullopt : std::optional(Package::TextureType::Texture3D);
    case glslang::EsdCube:
        return sampler.isArrayed() ? std::nullopt : std::optional(Package::TextureType::Cube);
    default:
        return std::nullopt;
    }
}

bool DeclarationReader::isBuiltInBlock(const glslang::TType& type) {
    if (type.getBasicType() != glslang::EbtBlock || type.getStruct() == nullptr || type.getStruct()->empty()) {
        return false;
    }
    return type.getStruct()->front().type->getQualifier().builtIn != glslang::EbvNone;
}

} // namespace haylen::shaderc
