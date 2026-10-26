#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "haylen/shaderc/Compiler.hpp"
#include "support/TargetValidator.hpp"

namespace haylen::shaderc::test {

// A shader of the Haylen engine or of its test project, with the variants the engine compiles it with.
struct HaylenShader {
    std::filesystem::path file;
    std::vector<Compiler::Variant> variants;
    std::string name;

    // Shows the shader by its name in the output of the tests.
    friend void PrintTo(const HaylenShader& shader, std::ostream* stream) {
        *stream << shader.name;
    }
};

class HaylenShadersTest : public ::testing::TestWithParam<HaylenShader> {
  public:
    static std::vector<HaylenShader> list() {
        const std::filesystem::path data = HAYLEN_SHADERC_TEST_DATA;
        std::vector<HaylenShader> shaders;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(data / "engine")) {
            if (declaresProgram(entry.path())) {
                shaders.push_back({.file = entry.path(), .variants = engineVariants(entry.path().stem().string()), .name = "engine_" + identifier(entry.path().stem().string())});
            }
        }
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(data / "samples")) {
            if (declaresProgram(entry.path())) {
                const std::string relative = std::filesystem::relative(entry.path(), data / "samples").replace_extension().generic_string();
                shaders.push_back({.file = entry.path(), .variants = materialVariants(), .name = "samples_" + identifier(relative)});
            }
        }
        std::ranges::sort(shaders, {}, &HaylenShader::name);
        return shaders;
    }

    static std::string nameOf(const ::testing::TestParamInfo<HaylenShader>& parameter) {
        return parameter.param.name;
    }

  protected:
    Compiler compiler;

    Package compileShader() const {
        const HaylenShader& shader = GetParam();
        return compiler.compile({.input = shader.file, .includes = {std::filesystem::path(HAYLEN_SHADERC_TEST_DATA) / "engine" / "include"}, .variants = shader.variants});
    }

    static const std::string& sourceOf(const Package& package, const Package::Function& function) {
        return package.sources[static_cast<std::size_t>(function.source)];
    }

  private:
    static bool declaresProgram(const std::filesystem::path& path) {
        if (path.extension() != ".glsl") {
            return false;
        }
        std::ifstream stream(path);
        const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        return text.starts_with("@program") || text.find("\n@program") != std::string::npos;
    }

    static std::vector<Compiler::Variant> materialVariants() {
        return {
            {.name = "sprite"}, {.name = "sprite_lit", .defines = {"HAYLEN_LIT"}}, {.name = "batch", .defines = {"HAYLEN_BATCH"}}, {.name = "batch_lit", .defines = {"HAYLEN_BATCH", "HAYLEN_LIT"}}, {.name = "text", .defines = {"HAYLEN_TEXT"}}, {.name = "text_lit", .defines = {"HAYLEN_TEXT", "HAYLEN_LIT"}}, {.name = "mesh", .defines = {"HAYLEN_MESH"}}, {.name = "mesh_lit", .defines = {"HAYLEN_MESH", "HAYLEN_LIT"}}, {.name = "shape", .defines = {"HAYLEN_SHAPE"}}, {.name = "shape_lit", .defines = {"HAYLEN_SHAPE", "HAYLEN_LIT"}},
        };
    }

    static std::vector<Compiler::Variant> engineVariants(const std::string& stem) {
        const std::map<std::string, std::vector<Compiler::Variant>> variants = {
            {"sprite", {{.name = "sprite"}, {.name = "sprite_lit", .defines = {"HAYLEN_LIT"}}, {.name = "batch", .defines = {"HAYLEN_BATCH"}}, {.name = "batch_lit", .defines = {"HAYLEN_BATCH", "HAYLEN_LIT"}}}}, {"text", {{.name = "text", .defines = {"HAYLEN_TEXT"}}, {.name = "text_lit", .defines = {"HAYLEN_TEXT", "HAYLEN_LIT"}}}}, {"mesh", {{.name = "mesh", .defines = {"HAYLEN_MESH"}}, {.name = "mesh_lit", .defines = {"HAYLEN_MESH", "HAYLEN_LIT"}}}}, {"shape", {{.name = "shape", .defines = {"HAYLEN_SHAPE"}}, {.name = "shape_lit", .defines = {"HAYLEN_SHAPE", "HAYLEN_LIT"}}}}, {"blend", {{.name = "blend"}, {.name = "blend_lit", .defines = {"HAYLEN_LIT"}}}}, {"metaball", {{.name = "metaball"}, {.name = "metaball_lit", .defines = {"HAYLEN_LIT"}}}}, {"recolor", {{.name = "recolor"}, {.name = "recolor_lit", .defines = {"HAYLEN_LIT"}}}}, {"effect", {{.name = "effect"}, {.name = "effect_lit", .defines = {"HAYLEN_LIT"}}}},
        };
        const auto found = variants.find(stem);
        return found == variants.end() ? std::vector<Compiler::Variant>{} : found->second;
    }

    static std::string identifier(std::string text) {
        std::ranges::replace_if(text, [](char character) { return !std::isalnum(static_cast<unsigned char>(character)); }, '_');
        return text;
    }
};

TEST_P(HaylenShadersTest, CompilesForEveryTargetAndPassesGlslangTintAndTheSpirvValidator) {
    const Package package = compileShader();

    if (!GetParam().variants.empty()) {
        ASSERT_EQ(package.programs.size(), GetParam().variants.size());
    }
    for (const Package::Program& program : package.programs) {
        ASSERT_EQ(program.targets.size(), Package::kTargets.size());
        EXPECT_GE(program.outputs, 1) << program.name;
        for (const Package::Code& code : program.targets) {
            if (code.target == Package::Target::Spirv) {
                EXPECT_EQ(TargetValidator::validateSpirv(sourceOf(package, code.vertex)), "") << program.name;
                EXPECT_EQ(TargetValidator::validateSpirv(sourceOf(package, code.fragment)), "") << program.name;
                continue;
            }
            EXPECT_NE(sourceOf(package, code.vertex).find(code.vertex.entry + "("), std::string::npos);
            EXPECT_NE(sourceOf(package, code.fragment).find(code.fragment.entry + "("), std::string::npos);
            if (Package::isGlsl(code.target)) {
                EXPECT_EQ(TargetValidator::validateGlsl(sourceOf(package, code.vertex), sourceOf(package, code.fragment)), "") << program.name << " " << Package::targetName(code.target);
            } else if (code.target == Package::Target::Wgsl) {
                EXPECT_EQ(TargetValidator::validateWgsl(sourceOf(package, code.vertex)), "") << program.name;
                EXPECT_EQ(TargetValidator::validateWgsl(sourceOf(package, code.fragment)), "") << program.name;
            }
        }
    }
}

TEST_P(HaylenShadersTest, PassesTheMetalCompilerWhenItIsInstalled) {
    if (!TargetValidator::canValidateMetal()) {
        GTEST_SKIP() << "The Metal toolchain of Xcode is not installed, so the Metal sources are not compiled.";
    }
    const Package package = compileShader();

    for (const Package::Target target : {Package::Target::MslMacos, Package::Target::MslIos}) {
        std::set<int> indices;
        for (const Package::Program& program : package.programs) {
            const Package::Code& code = *std::ranges::find(program.targets, target, &Package::Code::target);
            indices.insert(code.vertex.source);
            indices.insert(code.fragment.source);
        }
        std::vector<std::string> sources;
        for (const int index : indices) {
            sources.push_back(package.sources[static_cast<std::size_t>(index)]);
        }
        EXPECT_EQ(TargetValidator::validateMetal(sources, target), "") << Package::targetName(target);
    }
}

TEST_P(HaylenShadersTest, PassesFxcWhenItIsInstalled) {
    if (!TargetValidator::canValidateHlsl()) {
        GTEST_SKIP() << "The HLSL compiler \"fxc\" is not on the path, so the HLSL sources are not compiled.";
    }
    const Package package = compileShader();

    for (const Package::Program& program : package.programs) {
        const Package::Code& code = *std::ranges::find(program.targets, Package::Target::Hlsl5, &Package::Code::target);
        EXPECT_EQ(TargetValidator::validateHlsl(sourceOf(package, code.vertex), code.vertex.profile, code.vertex.entry), "") << program.name;
        EXPECT_EQ(TargetValidator::validateHlsl(sourceOf(package, code.fragment), code.fragment.profile, code.fragment.entry), "") << program.name;
    }
}

INSTANTIATE_TEST_SUITE_P(Haylen, HaylenShadersTest, ::testing::ValuesIn(HaylenShadersTest::list()), HaylenShadersTest::nameOf);

TEST(HaylenEngineShadersTest, KeepsTheTypesOfTheLightTextures) {
    const Compiler compiler;
    const Package package = compiler.compile({.input = std::filesystem::path(HAYLEN_SHADERC_TEST_DATA) / "engine" / "light.glsl", .targets = {Package::Target::Wgsl}});

    const Package::Program& program = package.programs.at(0);
    const auto texture = std::ranges::find(program.textures, "shadow_texture", &Package::Texture::name);
    const auto sampler = std::ranges::find(program.samplers, "shadow_sampler", &Package::Sampler::name);
    ASSERT_NE(texture, program.textures.end());
    ASSERT_NE(sampler, program.samplers.end());
    EXPECT_EQ(texture->sampleType, Package::SampleType::UnfilterableFloat);
    EXPECT_EQ(sampler->type, Package::SamplerType::Nonfiltering);
}

TEST(HaylenEngineShadersTest, CompilesTheLitSpriteBatchWithFourOutputs) {
    const Compiler compiler;
    const std::filesystem::path data = HAYLEN_SHADERC_TEST_DATA;
    const Package package = compiler.compile({.input = data / "engine" / "sprite.glsl", .includes = {data / "engine" / "include"}, .variants = {{.name = "batch_lit", .defines = {"HAYLEN_BATCH", "HAYLEN_LIT"}}}, .targets = {Package::Target::Glsl300es}});

    const Package::Program& program = package.programs.at(0);
    EXPECT_EQ(program.outputs, 4);
    EXPECT_EQ(program.attributes.size(), 11U);
    EXPECT_EQ(program.textures.size(), 9U);
    EXPECT_EQ(program.blocks.at(0).size, 80);
    EXPECT_EQ(program.blocks.at(1).slot, 7);
}

TEST(HaylenEngineShadersTest, BindsTheTexturesOfTheLitSpriteBatchWithinTheOldestAppleGpus) {
    const Compiler compiler;
    const std::filesystem::path data = HAYLEN_SHADERC_TEST_DATA;
    const Package package = compiler.compile({.input = data / "engine" / "sprite.glsl", .includes = {data / "engine" / "include"}, .variants = {{.name = "batch_lit", .defines = {"HAYLEN_BATCH", "HAYLEN_LIT"}}}, .targets = {Package::Target::MslIos}});

    // The A8 and A10X GPUs bind textures from 0 to 30 and samplers from 0 to 15, while the batch reads its last texture at slot 31.
    const Package::Program& program = package.programs.at(0);
    const Package::Code& code = program.targets.at(0);
    ASSERT_TRUE(code.textures.contains(31));
    for (const Package::Texture& texture : program.textures) {
        const auto before = std::ranges::count_if(program.textures, [&texture](const Package::Texture& other) { return other.stage == texture.stage && other.slot < texture.slot; });
        EXPECT_EQ(code.textures.at(texture.slot).binding, before) << texture.name;
        EXPECT_LE(code.textures.at(texture.slot).binding, 30) << texture.name;
    }
    for (const Package::Sampler& sampler : program.samplers) {
        EXPECT_LE(code.samplers.at(sampler.slot).binding, 15) << sampler.name;
    }
}

} // namespace haylen::shaderc::test
