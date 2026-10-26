#include <algorithm>
#include <cstdint>
#include <format>
#include <iterator>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/CompilerFixture.hpp"
#include "support/SpirvModule.hpp"
#include "support/TargetValidator.hpp"

namespace haylen::shaderc::test {

class TargetTest : public CompilerFixture {
  protected:
    using Bindings = std::map<int, Package::Binding>;

    // A program with uniform blocks, textures and samplers in both stages at scattered slots, an integer block and vertex inputs.
    static constexpr std::string_view kSource = R"(@vs main_vs
layout(binding=2) uniform vertex_params {
    mat4 view_projection;
};
layout(binding=6) uniform texture2D height_texture;
layout(binding=3) uniform sampler height_sampler;
in vec2 corner;
in vec4 tint;
out vec4 color;
out vec2 uv;
void main() {
    float height = textureLod(sampler2D(height_texture, height_sampler), corner, 0.0).x;
    color = tint;
    uv = corner;
    gl_Position = view_projection * vec4(corner, height, 1.0);
}
@end

@fs main_fs
layout(binding=1) uniform fragment_params {
    vec4 fade;
    float strength;
};
layout(binding=5) uniform fragment_counts {
    ivec4 steps;
};
layout(binding=3) uniform texture2D color_texture;
layout(binding=9) uniform texture2D detail_texture;
layout(binding=1) uniform sampler color_sampler;
layout(binding=4) uniform sampler detail_sampler;
in vec4 color;
in vec2 uv;
out vec4 frag_color;
void main() {
    vec4 base = texture(sampler2D(color_texture, color_sampler), uv);
    vec4 detail = texture(sampler2D(detail_texture, detail_sampler), uv * float(steps.x));
    frag_color = base * detail * color * fade * strength;
}
@end

@program main main_vs main_fs
)";

    // A fragment stage that takes every derivative of GLSL, which the GLSL ES target cannot translate.
    static constexpr std::string_view kDerivativeSource = R"(@vs main_vs
in vec2 corner;
out vec2 uv;
void main() {
    uv = corner;
    gl_Position = vec4(corner, 0.0, 1.0);
}
@end
@fs main_fs
in vec2 uv;
out vec4 frag_color;
void main() {
    vec2 plain = dFdx(uv) + dFdy(uv) + fwidth(uv);
    vec2 fine = dFdxFine(uv) + dFdyFine(uv) + fwidthFine(uv);
    vec2 coarse = dFdxCoarse(uv) + dFdyCoarse(uv) + fwidthCoarse(uv);
    frag_color = vec4(plain + fine, coarse);
}
@end
@program main main_vs main_fs
)";

    // Counts the matches of a pattern in a source.
    static std::ptrdiff_t count(const std::string& text, const std::string& pattern) {
        const std::regex expression(pattern);
        return std::distance(std::sregex_iterator(text.begin(), text.end(), expression), std::sregex_iterator());
    }

    // The bindings of the targets with descriptor sets, SPIR-V and WGSL.
    static inline const Bindings kSetBlocks = {{1, {.set = 0, .binding = 1}}, {2, {.set = 0, .binding = 2}}, {5, {.set = 0, .binding = 5}}};
    static inline const Bindings kSetTextures = {{3, {.set = 1, .binding = 3}}, {6, {.set = 1, .binding = 6}}, {9, {.set = 1, .binding = 9}}};
    static inline const Bindings kSetSamplers = {{1, {.set = 1, .binding = 65}}, {3, {.set = 1, .binding = 67}}, {4, {.set = 1, .binding = 68}}};

    static const Package::Code& codeFor(const Package& package, Package::Target target) {
        return *std::ranges::find(package.programs[0].targets, target, &Package::Code::target);
    }

    static const std::string& vertexOf(const Package& package, Package::Target target) {
        return package.sources[static_cast<std::size_t>(codeFor(package, target).vertex.source)];
    }

    static const std::string& fragmentOf(const Package& package, Package::Target target) {
        return package.sources[static_cast<std::size_t>(codeFor(package, target).fragment.source)];
    }

    static bool contains(const std::string& text, std::string_view part) {
        return text.find(part) != std::string::npos;
    }
};

TEST_F(TargetTest, CompilesTheRequestedTargetsInTheirOwnOrder) {
    const Package package = compile(kSource, {.targets = {Package::Target::Wgsl, Package::Target::MslIos}});

    ASSERT_EQ(package.programs[0].targets.size(), 2U);
    EXPECT_EQ(package.programs[0].targets[0].target, Package::Target::MslIos);
    EXPECT_EQ(package.programs[0].targets[1].target, Package::Target::Wgsl);
}

TEST_F(TargetTest, MetalNumbersTheResourcesOfEachStageFromZero) {
    const Package package = compile(kSource);

    for (const Package::Target target : {Package::Target::MslMacos, Package::Target::MslIos, Package::Target::MslIosSimulator}) {
        const Package::Code& code = codeFor(package, target);
        EXPECT_EQ(code.blocks, (Bindings{{1, {.binding = 0}}, {2, {.binding = 0}}, {5, {.binding = 1}}}));
        EXPECT_EQ(code.textures, (Bindings{{3, {.binding = 0}}, {6, {.binding = 0}}, {9, {.binding = 1}}}));
        EXPECT_EQ(code.samplers, (Bindings{{1, {.binding = 0}}, {3, {.binding = 0}}, {4, {.binding = 1}}}));
        EXPECT_EQ(code.vertex.entry, "main0");
        EXPECT_EQ(code.fragment.entry, "main0");
    }

    const std::string& vertex = vertexOf(package, Package::Target::MslMacos);
    const std::string& fragment = fragmentOf(package, Package::Target::MslMacos);
    EXPECT_TRUE(contains(vertex, "[[stage_in]]")) << vertex;
    EXPECT_TRUE(contains(vertex, "float2 corner [[attribute(0)]];")) << vertex;
    EXPECT_TRUE(contains(vertex, "float4 tint [[attribute(1)]];")) << vertex;
    EXPECT_TRUE(contains(vertex, "[[buffer(0)]]")) << vertex;
    EXPECT_TRUE(contains(vertex, "[[texture(0)]]")) << vertex;
    EXPECT_TRUE(contains(vertex, "[[sampler(0)]]")) << vertex;
    EXPECT_TRUE(contains(vertex, "vertex main0_out main0(")) << vertex;
    EXPECT_TRUE(contains(fragment, "[[buffer(0)]]")) << fragment;
    EXPECT_TRUE(contains(fragment, "[[buffer(1)]]")) << fragment;
    EXPECT_TRUE(contains(fragment, "[[texture(0)]]")) << fragment;
    EXPECT_TRUE(contains(fragment, "[[texture(1)]]")) << fragment;
    EXPECT_TRUE(contains(fragment, "[[sampler(0)]]")) << fragment;
    EXPECT_TRUE(contains(fragment, "[[sampler(1)]]")) << fragment;
    EXPECT_TRUE(contains(fragment, "fragment main0_out main0(")) << fragment;
}

TEST_F(TargetTest, IosTargetsShareTheirSources) {
    const Package package = compile(kSource);

    EXPECT_EQ(codeFor(package, Package::Target::MslIos).vertex.source, codeFor(package, Package::Target::MslIosSimulator).vertex.source);
    EXPECT_EQ(codeFor(package, Package::Target::MslIos).fragment.source, codeFor(package, Package::Target::MslIosSimulator).fragment.source);
}

TEST_F(TargetTest, HlslUsesTheRegistersOfEachStageAndTexcoordSemantics) {
    const Package package = compile(kSource);

    const Package::Code& code = codeFor(package, Package::Target::Hlsl5);
    EXPECT_EQ(code.blocks, (Bindings{{1, {.binding = 0}}, {2, {.binding = 0}}, {5, {.binding = 1}}}));
    EXPECT_EQ(code.textures, (Bindings{{3, {.binding = 3}}, {6, {.binding = 6}}, {9, {.binding = 9}}}));
    EXPECT_EQ(code.samplers, (Bindings{{1, {.binding = 1}}, {3, {.binding = 3}}, {4, {.binding = 4}}}));
    EXPECT_EQ(code.vertex.entry, "main");
    EXPECT_EQ(code.vertex.profile, "vs_5_0");
    EXPECT_EQ(code.fragment.profile, "ps_5_0");

    const std::string& vertex = vertexOf(package, Package::Target::Hlsl5);
    const std::string& fragment = fragmentOf(package, Package::Target::Hlsl5);
    EXPECT_TRUE(contains(vertex, "float2 corner : TEXCOORD0;")) << vertex;
    EXPECT_TRUE(contains(vertex, "float4 tint : TEXCOORD1;")) << vertex;
    EXPECT_TRUE(contains(vertex, "register(b0)")) << vertex;
    EXPECT_TRUE(contains(vertex, "register(t6)")) << vertex;
    EXPECT_TRUE(contains(vertex, "register(s3)")) << vertex;
    EXPECT_TRUE(contains(fragment, "register(b0)")) << fragment;
    EXPECT_TRUE(contains(fragment, "register(b1)")) << fragment;
    EXPECT_TRUE(contains(fragment, "register(t9)")) << fragment;
    EXPECT_TRUE(contains(fragment, "register(s4)")) << fragment;
}

TEST_F(TargetTest, GlslFlattensBlocksIntoArraysOfVectors) {
    const Package package = compile(kSource);

    for (const Package::Target target : {Package::Target::Glsl430, Package::Target::Glsl300es}) {
        const Package::Code& code = codeFor(package, target);
        ASSERT_EQ(code.flatBlocks.size(), 3U);
        EXPECT_EQ(code.flatBlocks.at(2).name, "vertex_params");
        EXPECT_EQ(code.flatBlocks.at(2).type, "vec4");
        EXPECT_EQ(code.flatBlocks.at(2).count, 4);
        EXPECT_EQ(code.flatBlocks.at(1).name, "fragment_params");
        EXPECT_EQ(code.flatBlocks.at(1).count, 2);
        EXPECT_EQ(code.flatBlocks.at(5).type, "ivec4");
        EXPECT_EQ(code.flatBlocks.at(5).count, 1);
        EXPECT_TRUE(code.blocks.empty());
        EXPECT_TRUE(code.textures.empty());
        EXPECT_EQ(code.vertex.entry, "main");
    }

    EXPECT_TRUE(contains(vertexOf(package, Package::Target::Glsl430), "uniform vec4 vertex_params[4];"));
    EXPECT_TRUE(contains(fragmentOf(package, Package::Target::Glsl430), "uniform vec4 fragment_params[2];"));
    EXPECT_TRUE(contains(fragmentOf(package, Package::Target::Glsl430), "uniform ivec4 fragment_counts[1];"));
}

TEST_F(TargetTest, GlslCombinesTexturesAndSamplersByName) {
    const Package package = compile(kSource);

    for (const Package::Target target : {Package::Target::Glsl430, Package::Target::Glsl300es}) {
        EXPECT_TRUE(contains(vertexOf(package, target), "sampler2D height_texture_height_sampler;")) << vertexOf(package, target);
        EXPECT_TRUE(contains(fragmentOf(package, target), "sampler2D color_texture_color_sampler;"));
        EXPECT_TRUE(contains(fragmentOf(package, target), "sampler2D detail_texture_detail_sampler;"));
        EXPECT_FALSE(contains(fragmentOf(package, target), "binding"));
    }
}

TEST_F(TargetTest, GlslKeepsTheNamesOfVertexInputs) {
    const Package package = compile(kSource);

    EXPECT_TRUE(contains(vertexOf(package, Package::Target::Glsl430), "layout(location = 0) in vec2 corner;"));
    EXPECT_TRUE(contains(vertexOf(package, Package::Target::Glsl300es), "layout(location = 1) in vec4 tint;"));
}

TEST_F(TargetTest, GlslMapsClipSpaceDepthToOpenGl) {
    const Package package = compile(kSource);

    EXPECT_TRUE(contains(vertexOf(package, Package::Target::Glsl430), "#version 430"));
    EXPECT_TRUE(contains(vertexOf(package, Package::Target::Glsl300es), "#version 300 es"));
    EXPECT_TRUE(contains(fragmentOf(package, Package::Target::Glsl300es), "precision highp float;"));
    for (const Package::Target target : {Package::Target::Glsl430, Package::Target::Glsl300es}) {
        EXPECT_TRUE(contains(vertexOf(package, target), "gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;")) << vertexOf(package, target);
    }
    EXPECT_FALSE(contains(vertexOf(package, Package::Target::MslMacos), "2.0 * gl_Position.z"));
}

TEST_F(TargetTest, GlslRejectsBlocksThatMixFloatsAndIntegers) {
    const std::string source = R"(@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
layout(binding=0) uniform mixed {
    vec4 tint;
    int steps;
};
out vec4 frag_color;
void main() {
    frag_color = tint * float(steps);
}
@end
@program main main_vs main_fs
)";

    EXPECT_EQ(errors(source), std::vector<std::string>{"shader.glsl:7:8: error: The uniform block \"mixed\" holds both floats and integers, which the GLSL targets cannot flatten into one array. Split it into a block of floats and a block of integers."});
    EXPECT_EQ(compile(source, {.targets = {Package::Target::MslMacos, Package::Target::Hlsl5, Package::Target::Wgsl}}).programs[0].targets.size(), 3U);
}

TEST_F(TargetTest, WgslBindsBlocksInGroupZeroAndResourcesInGroupOne) {
    const Package package = compile(kSource);

    const Package::Code& code = codeFor(package, Package::Target::Wgsl);
    EXPECT_EQ(code.blocks, kSetBlocks);
    EXPECT_EQ(code.textures, kSetTextures);
    EXPECT_EQ(code.samplers, kSetSamplers);
    EXPECT_EQ(code.vertex.entry, "main");
    EXPECT_EQ(code.fragment.entry, "main");

    const std::string& vertex = vertexOf(package, Package::Target::Wgsl);
    const std::string& fragment = fragmentOf(package, Package::Target::Wgsl);
    EXPECT_TRUE(contains(vertex, "@group(0u) @binding(2u)")) << vertex;
    EXPECT_TRUE(contains(vertex, "@group(1u) @binding(6u)")) << vertex;
    EXPECT_TRUE(contains(vertex, "@group(1u) @binding(67u)")) << vertex;
    EXPECT_TRUE(contains(vertex, "@vertex")) << vertex;
    EXPECT_TRUE(contains(fragment, "@group(0u) @binding(1u)")) << fragment;
    EXPECT_TRUE(contains(fragment, "@group(0u) @binding(5u)")) << fragment;
    EXPECT_TRUE(contains(fragment, "@group(1u) @binding(9u)")) << fragment;
    EXPECT_TRUE(contains(fragment, "@group(1u) @binding(68u)")) << fragment;
    EXPECT_TRUE(contains(fragment, "@fragment")) << fragment;
}

TEST_F(TargetTest, HlslTakesFineDerivativesExceptTheCoarseForms) {

    const Package package = compile(kDerivativeSource, {.targets = {Package::Target::Hlsl5}});

    // Direct3D computes its plain derivatives coarsely, while the other graphics APIs compute them for every pixel.
    const std::string& fragment = fragmentOf(package, Package::Target::Hlsl5);
    EXPECT_EQ(count(fragment, R"(\bddx\()"), 0) << fragment;
    EXPECT_EQ(count(fragment, R"(\bddy\()"), 0) << fragment;
    EXPECT_EQ(count(fragment, R"(\bddx_fine\()"), 4) << fragment;
    EXPECT_EQ(count(fragment, R"(\bddy_fine\()"), 4) << fragment;
    EXPECT_EQ(count(fragment, R"(\babs\(ddx_fine\()"), 2) << fragment;
    EXPECT_EQ(count(fragment, R"(\bddx_coarse\()"), 1) << fragment;
    EXPECT_EQ(count(fragment, R"(\bddy_coarse\()"), 1) << fragment;
    EXPECT_EQ(count(fragment, R"(\bfwidth\()"), 1) << fragment;
}

TEST_F(TargetTest, WgslTakesFineDerivativesExceptTheCoarseForms) {
    const Package package = compile(kDerivativeSource, {.targets = {Package::Target::Wgsl}});

    // WebGPU may compute its plain derivatives coarsely, while the other graphics APIs compute them for every pixel.
    const std::string& fragment = fragmentOf(package, Package::Target::Wgsl);
    EXPECT_EQ(count(fragment, R"(\b(dpdx|dpdy|fwidth)\()"), 0) << fragment;
    EXPECT_EQ(count(fragment, R"(\bdpdxFine\()"), 2) << fragment;
    EXPECT_EQ(count(fragment, R"(\bdpdyFine\()"), 2) << fragment;
    EXPECT_EQ(count(fragment, R"(\bfwidthFine\()"), 2) << fragment;
    EXPECT_EQ(count(fragment, R"(\bdpdxCoarse\()"), 1) << fragment;
    EXPECT_EQ(count(fragment, R"(\bdpdyCoarse\()"), 1) << fragment;
    EXPECT_EQ(count(fragment, R"(\bfwidthCoarse\()"), 1) << fragment;
    EXPECT_EQ(TargetValidator::validateWgsl(fragment), "");
}

TEST_F(TargetTest, VertexStagesPassOnlyTheOutputsThatTheFragmentStageDeclares) {
    const std::string source = R"(@vs main_vs
in vec2 corner;
out vec4 first;
out vec4 second;
out vec4 third;
void main() {
    first = vec4(1.0);
    second = vec4(2.0);
    third = vec4(3.0);
    gl_Position = vec4(corner, 0.0, 1.0);
}
@end
@fs main_fs
in vec4 first;
in vec4 third;
out vec4 frag_color;
void main() {
    frag_color = first + third;
}
@end
@program main main_vs main_fs
)";

    const Package package = compile(source);

    // Direct3D links the stages by the order of their variables, so both stages of HLSL list the same semantics.
    const std::string& vertex = vertexOf(package, Package::Target::Hlsl5);
    const std::string& fragment = fragmentOf(package, Package::Target::Hlsl5);
    for (const std::string& text : {vertex, fragment}) {
        EXPECT_TRUE(contains(text, "float4 first : TEXCOORD0;")) << text;
        EXPECT_TRUE(contains(text, "float4 third : TEXCOORD2;")) << text;
        EXPECT_FALSE(contains(text, "TEXCOORD1")) << text;
    }
    EXPECT_FALSE(contains(vertexOf(package, Package::Target::MslMacos), "[[user(locn1)]]"));
    EXPECT_FALSE(contains(vertexOf(package, Package::Target::Glsl430), "out vec4 second;"));
    EXPECT_EQ(TargetValidator::validateGlsl(vertexOf(package, Package::Target::Glsl300es), fragmentOf(package, Package::Target::Glsl300es)), "");
    EXPECT_EQ(TargetValidator::validateWgsl(vertexOf(package, Package::Target::Wgsl)), "");
    EXPECT_EQ(TargetValidator::validateSpirv(vertexOf(package, Package::Target::Spirv)), "");
}

TEST_F(TargetTest, EveryTargetCompilesAVertexStageThatWritesThePointSize) {
    const std::string source = R"(@vs main_vs
in vec2 corner;
void main() {
    gl_PointSize = 1.0;
    gl_Position = vec4(corner, 0.0, 1.0);
}
@end
@fs main_fs
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
@end
@program main main_vs main_fs
)";

    const Package package = compile(source);

    EXPECT_TRUE(contains(vertexOf(package, Package::Target::MslMacos), "[[point_size]]"));
    EXPECT_TRUE(contains(vertexOf(package, Package::Target::Glsl300es), "gl_PointSize = 1.0;"));
    EXPECT_FALSE(contains(vertexOf(package, Package::Target::Hlsl5), "PSIZE"));
    EXPECT_EQ(TargetValidator::validateWgsl(vertexOf(package, Package::Target::Wgsl)), "");
    EXPECT_EQ(TargetValidator::validateSpirv(vertexOf(package, Package::Target::Spirv)), "");
}

TEST_F(TargetTest, EverySourceDefinesItsEntryPoints) {
    const Package package = compile(kSource);

    for (const Package::Code& code : package.programs[0].targets) {
        for (const Package::Function* function : {&code.vertex, &code.fragment}) {
            const std::string& source = package.sources[static_cast<std::size_t>(function->source)];
            const std::string text = code.target == Package::Target::Spirv ? SpirvModule::disassemble(source).value_or("") : source;
            const std::string entry = code.target == Package::Target::Spirv ? std::format("\"{}\"", function->entry) : function->entry + "(";
            EXPECT_TRUE(contains(text, entry)) << Package::targetName(code.target);
        }
    }
}

TEST_F(TargetTest, GlslSourcesPassGlslang) {
    const Package package = compile(kSource);

    for (const Package::Target target : {Package::Target::Glsl430, Package::Target::Glsl300es}) {
        EXPECT_EQ(TargetValidator::validateGlsl(vertexOf(package, target), fragmentOf(package, target)), "") << Package::targetName(target);
    }
}

TEST_F(TargetTest, WgslSourcesPassTint) {
    const Package package = compile(kSource);

    EXPECT_EQ(TargetValidator::validateWgsl(vertexOf(package, Package::Target::Wgsl)), "");
    EXPECT_EQ(TargetValidator::validateWgsl(fragmentOf(package, Package::Target::Wgsl)), "");
}

TEST_F(TargetTest, MetalSourcesPassTheMetalCompilerWhenItIsInstalled) {
    if (!TargetValidator::canValidateMetal()) {
        GTEST_SKIP() << "The Metal toolchain of Xcode is not installed, so the Metal sources are not compiled.";
    }
    const Package package = compile(kSource);

    for (const Package::Target target : {Package::Target::MslMacos, Package::Target::MslIos, Package::Target::MslIosSimulator}) {
        EXPECT_EQ(TargetValidator::validateMetal({vertexOf(package, target), fragmentOf(package, target)}, target), "") << Package::targetName(target);
    }
}

TEST_F(TargetTest, HlslSourcesPassFxcWhenItIsInstalled) {
    if (!TargetValidator::canValidateHlsl()) {
        GTEST_SKIP() << "The HLSL compiler \"fxc\" is not on the path, so the HLSL sources are not compiled.";
    }
    const Package package = compile(kSource);

    const Package::Code& code = codeFor(package, Package::Target::Hlsl5);
    for (const Package::Function* function : {&code.vertex, &code.fragment}) {
        EXPECT_EQ(TargetValidator::validateHlsl(package.sources[static_cast<std::size_t>(function->source)], function->profile, function->entry), "") << function->profile;
    }
}

TEST_F(TargetTest, SpirvBindsBlocksInSetZeroAndResourcesInSetOne) {
    const Package package = compile(kSource);

    const Package::Code& code = codeFor(package, Package::Target::Spirv);
    EXPECT_EQ(code.blocks, kSetBlocks);
    EXPECT_EQ(code.textures, kSetTextures);
    EXPECT_EQ(code.samplers, kSetSamplers);
    EXPECT_EQ(code.vertex.entry, "main");
    EXPECT_EQ(code.fragment.entry, "main");

    const std::optional<std::string> vertex = SpirvModule::disassemble(vertexOf(package, Package::Target::Spirv));
    const std::optional<std::string> fragment = SpirvModule::disassemble(fragmentOf(package, Package::Target::Spirv));
    ASSERT_TRUE(vertex && fragment);
    for (const std::string_view part : {"OpEntryPoint Vertex %main \"main\"", "OpDecorate %_ DescriptorSet 0", "OpDecorate %_ Binding 2", "OpDecorate %height_texture DescriptorSet 1", "OpDecorate %height_texture Binding 6", "OpDecorate %height_sampler DescriptorSet 1", "OpDecorate %height_sampler Binding 67"}) {
        EXPECT_TRUE(contains(*vertex, part)) << part << "\n" << *vertex;
    }
    for (const std::string_view part : {"OpEntryPoint Fragment %main \"main\"", "OpDecorate %detail_texture Binding 9", "OpDecorate %color_sampler Binding 65", "OpDecorate %detail_sampler Binding 68"}) {
        EXPECT_TRUE(contains(*fragment, part)) << part << "\n" << *fragment;
    }
}

TEST_F(TargetTest, SpirvKeepsTheLocationsOfVertexInputsAndTheClipSpace) {
    const Package package = compile(kSource);

    const std::optional<std::string> vertex = SpirvModule::disassemble(vertexOf(package, Package::Target::Spirv));
    ASSERT_TRUE(vertex);
    EXPECT_TRUE(contains(*vertex, "OpDecorate %corner Location 0")) << *vertex;
    EXPECT_TRUE(contains(*vertex, "OpDecorate %tint Location 1")) << *vertex;
    EXPECT_FALSE(contains(*vertex, "OpFNegate")) << *vertex;
}

TEST_F(TargetTest, SpirvWritesModulesAsBase64OfLittleEndianWords) {
    const Package package = compile(kSource, {.targets = {Package::Target::Spirv}});

    const std::string& text = vertexOf(package, Package::Target::Spirv);
    EXPECT_EQ(text.size() % 4, 0U);
    EXPECT_TRUE(text.starts_with("AwIjBw")) << text.substr(0, 16);
    const std::optional<std::vector<std::uint32_t>> words = SpirvModule::decode(text);
    ASSERT_TRUE(words);
    ASSERT_GE(words->size(), 5U);
    EXPECT_EQ((*words)[0], 0x07230203U);
    EXPECT_EQ((*words)[1], 0x00010000U);
}

TEST_F(TargetTest, SpirvModulesPassTheValidatorOfVulkan) {
    const Package package = compile(kSource);

    EXPECT_EQ(TargetValidator::validateSpirv(vertexOf(package, Package::Target::Spirv)), "");
    EXPECT_EQ(TargetValidator::validateSpirv(fragmentOf(package, Package::Target::Spirv)), "");
}

} // namespace haylen::shaderc::test
