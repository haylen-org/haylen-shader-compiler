#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/CompilerFixture.hpp"

namespace haylen::shaderc::test {

class LanguageTest : public CompilerFixture {
  protected:
    static constexpr std::string_view kStages = R"(@vs main_vs
in vec2 corner;
void main() {
    gl_Position = vec4(corner, 0.0, 1.0);
}
@end

@fs main_fs
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
@end
)";

    static std::string withProgram(std::string_view text) {
        return std::string(text) + "\n@program main main_vs main_fs\n";
    }
};

TEST_F(LanguageTest, CompilesEveryProgramUnderItsName) {
    const Package package = compile(std::string(kStages) + "@program first main_vs main_fs\n@program second main_vs main_fs\n");

    ASSERT_EQ(package.programs.size(), 2U);
    EXPECT_EQ(package.name, "shader");
    EXPECT_EQ(package.programs[0].name, "first");
    EXPECT_EQ(package.programs[1].name, "second");
    EXPECT_EQ(package.programs[0].targets.size(), Package::kTargets.size());
}

TEST_F(LanguageTest, IgnoresTagsInComments) {
    const Package package = compile(withProgram(std::string("// @vs commented\n/* The tags below are text.\n@fs commented\n@end */\n") + std::string(kStages)));

    EXPECT_EQ(package.programs.size(), 1U);
}

TEST_F(LanguageTest, AllowsCommentsAfterTags) {
    const Package package = compile("@vs main_vs // The vertex stage.\nvoid main() {\n    gl_Position = vec4(0.0);\n}\n@end\n@fs main_fs\nout vec4 frag_color;\nvoid main() {\n    frag_color = vec4(1.0);\n}\n@end\n@program main main_vs main_fs // The program.\n");

    EXPECT_EQ(package.programs.size(), 1U);
}

TEST_F(LanguageTest, ReportsUnknownTags) {
    const std::vector<std::string> found = errors(withProgram("@vs main_vs\n  @vertex_stage\nvoid main() {}\n@end\n@fs main_fs\nvoid main() {}\n@end\n"));

    EXPECT_EQ(found, std::vector<std::string>{"shader.glsl:2:3: error: The tag \"@vertex_stage\" does not exist. The tags are \"@vs\", \"@fs\", \"@block\", \"@end\", \"@include\", \"@include_block\", \"@program\", \"@sample_type\" and \"@sampler_type\"."});
}

TEST_F(LanguageTest, ReportsCodeOutsideSectionsOncePerGap) {
    const std::vector<std::string> found = errors(withProgram(std::string("// A comment is fine.\n  float stray;\nfloat other;\n") + std::string(kStages)));

    EXPECT_EQ(found, std::vector<std::string>{"shader.glsl:2:3: error: Code outside a stage or a block is never compiled. Move it into an \"@vs\", \"@fs\" or \"@block\" section."});
}

TEST_F(LanguageTest, ReportsSectionsWithoutEnd) {
    const std::vector<std::string> found = errors("@vs main_vs\nvoid main() {}\n");

    EXPECT_EQ(found, std::vector<std::string>{"shader.glsl:1:1: error: The vertex stage \"main_vs\" has no \"@end\"."});
}

TEST_F(LanguageTest, ReportsSectionsThatStartInsideAnother) {
    const std::vector<std::string> found = errors(withProgram("@vs main_vs\nvoid main() {}\n@fs main_fs\nvoid main() {}\n@end\n"));

    EXPECT_EQ(found, (std::vector<std::string>{"shader.glsl:3:1: error: The vertex stage \"main_vs\" has no \"@end\" before this tag."}));
}

TEST_F(LanguageTest, ReportsEndWithoutSection) {
    const std::vector<std::string> found = errors(withProgram(std::string(kStages) + "@end\n"));

    EXPECT_EQ(found, std::vector<std::string>{"shader.glsl:14:1: error: The tag \"@end\" has no stage or block to close."});
}

TEST_F(LanguageTest, ReportsWrongArguments) {
    const std::vector<std::string> found = errors(std::string(kStages) + "@block\n@end extra\n@block 2d_light\n@end\n@program main main_vs\n");

    const std::vector<std::string> expected = {
        "shader.glsl:14:1: error: The tag \"@block\" takes the name of the block, such as \"@block lighting\".",
        "shader.glsl:15:6: error: The tag \"@end\" takes no arguments.",
        "shader.glsl:16:8: error: The name \"2d_light\" is not an identifier. A name starts with a letter or an underscore and goes on with letters, digits and underscores.",
        "shader.glsl:18:1: error: The tag \"@program\" takes the name of the program, its vertex stage and its fragment stage, such as \"@program sprite sprite_vs sprite_fs\".",
    };
    EXPECT_EQ(found, expected);
}

TEST_F(LanguageTest, ReportsTagsInTheWrongPlace) {
    const std::vector<std::string> found = errors(withProgram("@include_block shared\n@vs main_vs\n@include other.glsl\n@program inner main_vs main_fs\nvoid main() {}\n@end\n@fs main_fs\nvoid main() {}\n@end\n"));

    const std::vector<std::string> expected = {
        "shader.glsl:1:1: error: The tag \"@include_block\" is only allowed inside a stage or a block.",
        "shader.glsl:3:1: error: The tag \"@include\" is only allowed outside stages and blocks.",
        "shader.glsl:4:1: error: The tag \"@program\" is only allowed outside stages and blocks.",
    };
    EXPECT_EQ(found, expected);
}

TEST_F(LanguageTest, ReportsNamesThatAreTaken) {
    const std::vector<std::string> found = errors(std::string(kStages) + "@fs main_vs\nvoid main() {}\n@end\n@program main main_vs main_fs\n");

    EXPECT_EQ(found, std::vector<std::string>{"shader.glsl:14:5: error: The name \"main_vs\" is already the vertex stage declared at line 1 of \"shader.glsl\"."});
}

TEST_F(LanguageTest, ReportsProgramsWithWrongStages) {
    const std::vector<std::string> found = errors(std::string(kStages) + "@program swapped main_fs main_vs\n@program missing main_vs other_fs\n");

    const std::vector<std::string> expected = {
        "shader.glsl:14:18: error: The stage \"main_fs\" is a fragment stage, but the second name of \"@program\" is the vertex stage.",
        "shader.glsl:14:26: error: The stage \"main_vs\" is a vertex stage, but the third name of \"@program\" is the fragment stage.",
        "shader.glsl:15:26: error: There is no stage named \"other_fs\".",
    };
    EXPECT_EQ(found, expected);
}

TEST_F(LanguageTest, ReportsProgramsDeclaredTwice) {
    const std::vector<std::string> found = errors(std::string(kStages) + "@program main main_vs main_fs\n@program main main_vs main_fs\n");

    EXPECT_EQ(found, std::vector<std::string>{"shader.glsl:15:10: error: The program \"main\" is already declared at line 14 of \"shader.glsl\"."});
}

TEST_F(LanguageTest, ReportsFilesWithoutPrograms) {
    const std::vector<std::string> found = errors(kStages);

    EXPECT_EQ(found.size(), 1U);
    EXPECT_TRUE(found.front().ends_with("shader.glsl: error: The file declares no program. Declare one with \"@program\", such as \"@program sprite sprite_vs sprite_fs\"."));
}

TEST_F(LanguageTest, PastesBlocksAndTheBlocksTheyInclude) {
    const Package package = compile(R"(@block color
vec4 tint() {
    return vec4(scale(), 0.0, 0.0, 1.0);
}
@end

@block scale
float scale() {
    return 0.5;
}
@end

@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end

@fs main_fs
@include_block scale
@include_block color
out vec4 frag_color;
void main() {
    frag_color = tint();
}
@end

@program main main_vs main_fs
)");

    EXPECT_EQ(package.programs.size(), 1U);
}

TEST_F(LanguageTest, ReportsUnknownAndRecursiveBlocks) {
    const std::vector<std::string> found = errors(withProgram(R"(@block loop
@include_block loop
@end
@vs main_vs
@include_block missing
void main() {}
@end
@fs main_fs
@include_block loop
void main() {}
@end
)"));

    const std::vector<std::string> expected = {
        "shader.glsl:5:16: error: There is no block named \"missing\".",
        "shader.glsl:2:16: error: The block \"loop\" includes itself.",
    };
    EXPECT_EQ(found, expected);
}

TEST_F(LanguageTest, IncludesNextToTheIncludingFileFirst) {
    write("lib/shared.glsl", "@include nested.glsl\n");
    write("lib/nested.glsl", "@block shared\nlayout(binding=1) uniform beside { vec4 value; };\n@end\n");
    write("first/lib/shared.glsl", "@block shared\nlayout(binding=1) uniform first { vec4 value; };\n@end\n");
    write("second/lib/shared.glsl", "@block shared\nlayout(binding=1) uniform second { vec4 value; };\n@end\n");
    const std::string source = withProgram(R"(@include lib/shared.glsl
@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
@include_block shared
out vec4 frag_color;
void main() {
    frag_color = value;
}
@end
)");
    const Compiler::Options options{.includes = {folder.getPath() / "first", folder.getPath() / "second"}};

    EXPECT_EQ(compile(source, options).programs[0].blocks[0].name, "beside");

    std::filesystem::remove(folder.getPath() / "lib" / "shared.glsl");
    EXPECT_EQ(compile(source, options).programs[0].blocks[0].name, "first");
}

TEST_F(LanguageTest, IncludesEveryFileOnce) {
    write("a.glsl", "@block shared\nfloat shared_value() {\n    return 1.0;\n}\n@end\n");
    write("b.glsl", "@include a.glsl\n");
    const Package package = compile(withProgram(R"(@include a.glsl
@include b.glsl
@include shader.glsl
@vs main_vs
@include_block shared
void main() {
    gl_Position = vec4(shared_value());
}
@end
@fs main_fs
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
@end
)"));

    EXPECT_EQ(package.programs.size(), 1U);
}

TEST_F(LanguageTest, ReportsMissingIncludes) {
    const std::vector<std::string> found = errors(withProgram(std::string("@include  lib/missing file.glsl\n") + std::string(kStages)));

    EXPECT_EQ(found, std::vector<std::string>{"shader.glsl:1:11: error: The file \"lib/missing file.glsl\" is neither next to this file nor in an include folder."});
}

TEST_F(LanguageTest, MapsCompilerErrorsToTheFilesTheyComeFrom) {
    write("lib/common.glsl", "// Shared code.\n@block shared\nfloat twice(float value) {\n    return value * undefined_name;\n}\n@end\n");
    const std::vector<std::string> found = errors(withProgram(R"(@include lib/common.glsl

@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end

@fs main_fs
@include_block shared
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
@end
)"));

    EXPECT_EQ(found, std::vector<std::string>{"lib/common.glsl:4:20: error: Undeclared identifier (\"undefined_name\")."});
}

TEST_F(LanguageTest, KeepsTheLinesOfTheStageAfterAPastedBlock) {
    write("lib/common.glsl", "@block shared\nfloat half_of(float value) {\n    return value * 0.5;\n}\n@end\n");
    const std::vector<std::string> found = errors(withProgram(R"(@include lib/common.glsl
@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
@include_block shared
out vec4 frag_color;
void main() {
    frag_color = vec4(half_of(1.0));
    int count = 1.5;
}
@end
)"));

    ASSERT_EQ(found.size(), 1U);
    EXPECT_EQ(found.front(), "shader.glsl:12:9: error: Cannot convert from \"const float\" to \"temp highp int\" (\"=\").");
}

TEST_F(LanguageTest, ReportsTheErrorsOfBothStages) {
    const std::vector<std::string> found = errors(withProgram("@vs main_vs\nvoid main() {\n    float value = vertex_error;\n}\n@end\n@fs main_fs\nvoid main() {\n    float value = fragment_error;\n}\n@end\n"));

    const std::vector<std::string> expected = {
        "shader.glsl:3:19: error: Undeclared identifier (\"vertex_error\").",
        "shader.glsl:8:19: error: Undeclared identifier (\"fragment_error\").",
    };
    EXPECT_EQ(found, expected);
}

TEST_F(LanguageTest, CompilesVariantsWithTheirDefines) {
    const std::string source = withProgram(R"(@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
layout(binding=0) uniform texture2D color_texture;
layout(binding=0) uniform sampler color_sampler;
#if defined(LIT) && defined(SHARP)
layout(binding=1) uniform texture2D normal_texture;
#endif
out vec4 frag_color;
void main() {
    frag_color = texture(sampler2D(color_texture, color_sampler), vec2(0.5));
}
@end
)");
    const Package package = compile(source, {.variants = {{.name = "plain"}, {.name = "lit", .defines = {"LIT", "SHARP"}}}});

    ASSERT_EQ(package.programs.size(), 2U);
    EXPECT_EQ(package.programs[0].name, "plain");
    EXPECT_EQ(package.programs[0].textures.size(), 1U);
    EXPECT_EQ(package.programs[1].name, "lit");
    EXPECT_EQ(package.programs[1].textures.size(), 2U);
}

TEST_F(LanguageTest, AllowsVariantsOnlyForOneProgram) {
    const std::vector<std::string> found = errors(std::string(kStages) + "@program first main_vs main_fs\n@program second main_vs main_fs\n", {.variants = {{.name = "lit", .defines = {"LIT"}}}});

    EXPECT_EQ(found, std::vector<std::string>{"shader.glsl:15:1: error: Variants compile the one program of a file, but this file declares 2 programs."});
}

TEST_F(LanguageTest, RejectsWrongVariantsAndTargets) {
    const std::string source = withProgram(kStages);

    EXPECT_THROW(static_cast<void>(compile(source, {.variants = {{.name = "lit"}, {.name = "lit"}}})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(compile(source, {.variants = {{.name = "lit glow"}}})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(compile(source, {.variants = {{.name = "lit", .defines = {"2D"}}}})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(compile(source, {.targets = {Package::Target::Wgsl, Package::Target::Wgsl}})), std::invalid_argument);
}

TEST_F(LanguageTest, SetsSampleAndSamplerTypes) {
    const Package package = compile(withProgram(R"(@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
@sample_type depth_texture depth
@sampler_type plain_sampler nonfiltering
layout(binding=0) uniform texture2D color_texture;
layout(binding=1) uniform texture2D depth_texture;
layout(binding=2) uniform itexture2D index_texture;
layout(binding=3) uniform utexture3D mask_texture;
layout(binding=0) uniform sampler color_sampler;
layout(binding=1) uniform sampler plain_sampler;
layout(binding=2) uniform samplerShadow shadow_sampler;
out vec4 frag_color;
void main() {
    frag_color = texture(sampler2D(color_texture, color_sampler), vec2(0.5));
}
@end
)"));

    const Package::Program& program = package.programs[0];
    ASSERT_EQ(program.textures.size(), 4U);
    EXPECT_EQ(program.textures[0].sampleType, Package::SampleType::Float);
    EXPECT_EQ(program.textures[1].sampleType, Package::SampleType::Depth);
    EXPECT_EQ(program.textures[2].sampleType, Package::SampleType::Sint);
    EXPECT_EQ(program.textures[3].sampleType, Package::SampleType::Uint);
    ASSERT_EQ(program.samplers.size(), 3U);
    EXPECT_EQ(program.samplers[0].type, Package::SamplerType::Filtering);
    EXPECT_EQ(program.samplers[1].type, Package::SamplerType::Nonfiltering);
    EXPECT_EQ(program.samplers[2].type, Package::SamplerType::Comparison);
}

TEST_F(LanguageTest, TagsTravelWithTheirBlocks) {
    const Package package = compile(withProgram(R"(@block shadows
@sample_type shadow_texture unfilterable_float
layout(binding=4) uniform texture2D shadow_texture;
@end
@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
@include_block shadows
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
@end
)"));

    EXPECT_EQ(package.programs[0].textures.at(0).sampleType, Package::SampleType::UnfilterableFloat);
}

TEST_F(LanguageTest, ReportsTagsWithUnknownOrConflictingTypes) {
    const std::vector<std::string> found = errors(withProgram(R"(@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
@sample_type color_texture filtered
@sampler_type color_sampler sharp
@sample_type depth_texture depth
@sample_type depth_texture unfilterable_float
layout(binding=0) uniform texture2D depth_texture;
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
@end
)"));

    const std::vector<std::string> expected = {
        "shader.glsl:7:28: error: The sample type \"filtered\" is not \"float\", \"unfilterable_float\", \"depth\", \"sint\" or \"uint\".",
        "shader.glsl:8:29: error: The sampler type \"sharp\" is not \"filtering\", \"nonfiltering\" or \"comparison\".",
        "shader.glsl:10:14: error: The texture \"depth_texture\" already has the sample type \"depth\" at line 9 of \"shader.glsl\".",
    };
    EXPECT_EQ(found, expected);
}

TEST_F(LanguageTest, ReportsTypeTagsThatDoNotFitTheirDeclarations) {
    const std::vector<std::string> found = errors(withProgram(R"(@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
@sample_type index_texture float
@sample_type color_texture sint
@sampler_type color_sampler comparison
@sampler_type shadow_sampler filtering
layout(binding=0) uniform texture2D color_texture;
layout(binding=1) uniform itexture2D index_texture;
layout(binding=0) uniform sampler color_sampler;
layout(binding=1) uniform samplerShadow shadow_sampler;
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
@end
)"));

    const std::vector<std::string> expected = {
        "shader.glsl:7:14: error: The texture \"index_texture\" holds signed integers, so its sample type is \"sint\", not \"float\".",
        "shader.glsl:8:14: error: The texture \"color_texture\" holds floats, so its sample type is \"float\", \"unfilterable_float\" or \"depth\", not \"sint\".",
        "shader.glsl:9:15: error: The sampler \"color_sampler\" is a \"sampler\", so its type is \"filtering\" or \"nonfiltering\", not \"comparison\". Declare it as a \"samplerShadow\" to compare depths.",
        "shader.glsl:10:15: error: The sampler \"shadow_sampler\" is a \"samplerShadow\", so its type is \"comparison\", not \"filtering\".",
    };
    EXPECT_EQ(found, expected);
}

TEST_F(LanguageTest, ReportsTagsThatNameNothing) {
    const std::vector<std::string> found = errors(withProgram(R"(@vs main_vs
void main() {
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
@sample_type missing_texture depth
@sampler_type missing_sampler nonfiltering
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
@end
)"));

    const std::vector<std::string> expected = {
        "shader.glsl:7:14: error: The fragment stage \"main_fs\" declares no texture named \"missing_texture\".",
        "shader.glsl:8:15: error: The fragment stage \"main_fs\" declares no sampler named \"missing_sampler\".",
    };
    EXPECT_EQ(found, expected);
}

} // namespace haylen::shaderc::test
