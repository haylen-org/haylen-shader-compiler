#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/CompilerFixture.hpp"

namespace haylen::shaderc::test {

class ReflectionTest : public CompilerFixture {
  protected:
    // A program whose fragment stage holds the declarations a test gives and writes one color.
    static std::string withFragment(std::string_view declarations, std::string_view body = "frag_color = vec4(1.0);") {
        return std::string("@vs main_vs\nvoid main() {\n    gl_Position = vec4(0.0);\n}\n@end\n@fs main_fs\n") + std::string(declarations) + "\nout vec4 frag_color;\nvoid main() {\n    " + std::string(body) + "\n}\n@end\n@program main main_vs main_fs\n";
    }

    static const Package::Code& codeFor(const Package::Program& program, Package::Target target) {
        return *std::ranges::find(program.targets, target, &Package::Code::target);
    }
};

TEST_F(ReflectionTest, LaysOutUniformBlocksWithStd140) {
    const Package package = compile(withFragment(R"(layout(binding=3) uniform params {
    float a;
    vec2 b;
    vec3 c;
    float d;
    vec4 e;
    mat4 f;
    float g[3];
    vec2 h;
};)"));

    ASSERT_EQ(package.programs[0].blocks.size(), 1U);
    const Package::Block& block = package.programs[0].blocks[0];
    EXPECT_EQ(block.name, "params");
    EXPECT_EQ(block.stage, Package::Stage::Fragment);
    EXPECT_EQ(block.slot, 3);
    EXPECT_EQ(block.size, 176);
    const std::vector<std::string> names = {"a", "b", "c", "d", "e", "f", "g", "h"};
    const std::vector<std::string> types = {"float", "vec2", "vec3", "float", "vec4", "mat4", "float", "vec2"};
    const std::vector<int> offsets = {0, 8, 16, 28, 32, 48, 112, 160};
    const std::vector<int> counts = {1, 1, 1, 1, 1, 1, 3, 1};
    ASSERT_EQ(block.members.size(), names.size());
    for (std::size_t index = 0; index < names.size(); ++index) {
        EXPECT_EQ(block.members[index].name, names[index]);
        EXPECT_EQ(block.members[index].type, types[index]);
        EXPECT_EQ(block.members[index].offset, offsets[index]) << names[index];
        EXPECT_EQ(block.members[index].count, counts[index]) << names[index];
    }
}

TEST_F(ReflectionTest, RoundsBlockSizesToWholeVectors) {
    const Package package = compile(withFragment("layout(binding=0) uniform small { float value; };\nlayout(binding=1) uniform counts { int first; ivec3 rest; };"));

    ASSERT_EQ(package.programs[0].blocks.size(), 2U);
    EXPECT_EQ(package.programs[0].blocks[0].size, 16);
    EXPECT_EQ(package.programs[0].blocks[1].size, 32);
    EXPECT_EQ(package.programs[0].blocks[1].members[1].offset, 16);
}

TEST_F(ReflectionTest, ReportsMembersOfUnsupportedTypes) {
    const std::vector<std::string> found = errors(withFragment(R"(struct Light { vec4 color; };
layout(binding=0) uniform params {
    mat3 rotation;
    uint count;
    Light light;
};)"));

    const std::vector<std::string> expected = {
        "shader.glsl:9:10: error: The member \"rotation\" of the uniform block \"params\" is a \"mat3\", but members are \"float\", \"vec2\", \"vec3\", \"vec4\", \"int\", \"ivec2\", \"ivec3\", \"ivec4\" or \"mat4\".",
        "shader.glsl:10:10: error: The member \"count\" of the uniform block \"params\" is a \"uint\", but members are \"float\", \"vec2\", \"vec3\", \"vec4\", \"int\", \"ivec2\", \"ivec3\", \"ivec4\" or \"mat4\".",
        "shader.glsl:11:11: error: The member \"light\" of the uniform block \"params\" is a \"Light\", but members are \"float\", \"vec2\", \"vec3\", \"vec4\", \"int\", \"ivec2\", \"ivec3\", \"ivec4\" or \"mat4\".",
    };
    EXPECT_EQ(found, expected);
}

TEST_F(ReflectionTest, ReflectsTexturesWithTheirTypes) {
    const Package package = compile(withFragment(R"(layout(binding=0) uniform texture2D flat_texture;
layout(binding=1) uniform texture2DArray layered_texture;
layout(binding=2) uniform texture3D volume_texture;
layout(binding=3) uniform textureCube sky_texture;)"));

    const std::vector<Package::Texture>& textures = package.programs[0].textures;
    ASSERT_EQ(textures.size(), 4U);
    EXPECT_EQ(textures[0].type, Package::TextureType::Texture2D);
    EXPECT_EQ(textures[1].type, Package::TextureType::Texture2DArray);
    EXPECT_EQ(textures[2].type, Package::TextureType::Texture3D);
    EXPECT_EQ(textures[3].type, Package::TextureType::Cube);
    EXPECT_EQ(textures[3].name, "sky_texture");
    EXPECT_EQ(textures[3].stage, Package::Stage::Fragment);
    EXPECT_EQ(textures[3].slot, 3);
}

TEST_F(ReflectionTest, ReportsDeclarationsTheLanguageDoesNotSupport) {
    const std::vector<std::string> found = errors(withFragment(R"(layout(binding=0) uniform texture2DMS multisampled_texture;
layout(binding=1) uniform sampler2D combined_texture;
layout(binding=2, rgba8) uniform image2D storage_image;
layout(binding=3) buffer storage { vec4 values[]; };
layout(binding=4) uniform texture2D textures[2];
layout(push_constant) uniform constants { vec4 value; };
layout(set=1, binding=5) uniform texture2D other_set;)"));

    const std::vector<std::string> expected = {
        "shader.glsl:8:37: error: The combined texture sampler \"combined_texture\" is not part of the language. Declare a texture and a sampler apart and combine them where the stage samples, such as \"texture(sampler2D(color_texture, color_sampler), uv)\".", "shader.glsl:9:42: error: Storage images such as \"storage_image\" are not supported.", "shader.glsl:10:8: error: Storage buffers such as \"storage\" are not supported.", "shader.glsl:11:37: error: Arrays of textures or samplers such as \"textures\" are not supported. Declare each one apart.", "shader.glsl:12:8: error: Push constants such as \"constants\" are not supported. Declare a uniform block with a binding instead.", "shader.glsl:13:44: error: The language has no descriptor sets, so \"other_set\" cannot take \"set\". Remove it from its layout.", "shader.glsl:7:39: error: The texture \"multisampled_texture\" is a \"texture2DMS\", but textures are \"texture2D\", \"texture2DArray\", \"texture3D\" or \"textureCube\", with \"i\" or \"u\" in front for integers.",
    };
    EXPECT_EQ(found, expected);
}

TEST_F(ReflectionTest, ReflectsSamplersAndThePairsTheStagesSampleWith) {
    const Package package = compile(R"(@vs main_vs
layout(binding=4) uniform texture2D height_texture;
layout(binding=2) uniform sampler height_sampler;
void main() {
    gl_Position = vec4(textureLod(sampler2D(height_texture, height_sampler), vec2(0.5), 0.0).x);
}
@end
@fs main_fs
layout(binding=0) uniform texture2D color_texture;
layout(binding=1) uniform texture2D unused_texture;
layout(binding=0) uniform sampler color_sampler;
layout(binding=1) uniform sampler point_sampler;
out vec4 frag_color;
void main() {
    frag_color = texture(sampler2D(color_texture, point_sampler), vec2(0.5)) + texture(sampler2D(color_texture, color_sampler), vec2(0.5));
}
@end
@program main main_vs main_fs
)");

    const Package::Program& program = package.programs[0];
    ASSERT_EQ(program.textures.size(), 3U);
    EXPECT_EQ(program.textures[2].name, "height_texture");
    EXPECT_EQ(program.textures[2].stage, Package::Stage::Vertex);
    ASSERT_EQ(program.samplers.size(), 3U);
    EXPECT_EQ(program.samplers[0].name, "color_sampler");
    EXPECT_EQ(program.samplers[2].stage, Package::Stage::Vertex);

    ASSERT_EQ(program.pairs.size(), 3U);
    EXPECT_EQ(program.pairs[0].stage, Package::Stage::Vertex);
    EXPECT_EQ(program.pairs[0].texture, 4);
    EXPECT_EQ(program.pairs[0].sampler, 2);
    EXPECT_EQ(program.pairs[0].name, "height_texture_height_sampler");
    EXPECT_EQ(program.pairs[1].name, "color_texture_color_sampler");
    EXPECT_EQ(program.pairs[2].name, "color_texture_point_sampler");
    EXPECT_EQ(program.pairs[2].sampler, 1);
}

TEST_F(ReflectionTest, NumbersVertexInputsInDeclarationOrder) {
    const Package package = compile(R"(@vs main_vs
in vec2 corner;
layout(location=1) in vec4 fixed_color;
in float rotation;
in ivec2 cell;
in uvec4 flags;
void main() {
    gl_Position = vec4(corner, rotation, float(cell.x + int(flags.x))) * fixed_color;
}
@end
@fs main_fs
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
@end
@program main main_vs main_fs
)");

    const std::vector<Package::Attribute>& attributes = package.programs[0].attributes;
    ASSERT_EQ(attributes.size(), 5U);
    const std::vector<std::string> names = {"corner", "fixed_color", "rotation", "cell", "flags"};
    const std::vector<std::string> types = {"vec2", "vec4", "float", "ivec2", "uvec4"};
    for (std::size_t index = 0; index < names.size(); ++index) {
        EXPECT_EQ(attributes[index].location, static_cast<int>(index));
        EXPECT_EQ(attributes[index].name, names[index]);
        EXPECT_EQ(attributes[index].type, types[index]);
    }
}

TEST_F(ReflectionTest, GivesFragmentInputsTheLocationsOfTheVertexOutputs) {
    const Package package = compile(R"(@vs main_vs
out vec2 uv;
out vec4 tint;
out float fade;
void main() {
    uv = vec2(0.0);
    tint = vec4(1.0);
    fade = 1.0;
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
in float fade;
in vec2 uv;
in vec4 tint;
out vec4 frag_color;
void main() {
    frag_color = tint * fade + vec4(uv, 0.0, 0.0);
}
@end
@program main main_vs main_fs
)",
                                    {.targets = {Package::Target::Glsl430}});

    const std::string& fragment = package.sources[static_cast<std::size_t>(package.programs[0].targets[0].fragment.source)];
    EXPECT_NE(fragment.find("layout(location = 2) in float fade;"), std::string::npos) << fragment;
    EXPECT_NE(fragment.find("layout(location = 0) in vec2 uv;"), std::string::npos) << fragment;
    EXPECT_NE(fragment.find("layout(location = 1) in vec4 tint;"), std::string::npos) << fragment;
}

TEST_F(ReflectionTest, ReportsFragmentInputsWithoutVertexOutputs) {
    const std::vector<std::string> found = errors(R"(@vs main_vs
out vec2 uv;
out vec3 normal;
layout(location=4) out vec4 tint;
void main() {
    gl_Position = vec4(0.0);
}
@end
@fs main_fs
in vec2 missing;
in vec2 normal;
layout(location=2) in vec4 tint;
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
@end
@program main main_vs main_fs
)");

    const std::vector<std::string> expected = {
        "shader.glsl:10:9: error: The fragment input \"missing\" has no vertex output with the same name.",
        "shader.glsl:11:9: error: The fragment input \"normal\" is a \"vec2\", but the vertex output with the same name is a \"vec3\".",
        "shader.glsl:12:28: error: The fragment input \"tint\" is at location 2, but the vertex output with the same name is at location 4.",
    };
    EXPECT_EQ(found, expected);
}

TEST_F(ReflectionTest, CountsTheColorOutputs) {
    const Package single = compile(withFragment(""));
    const Package several = compile("@vs main_vs\nvoid main() {\n    gl_Position = vec4(0.0);\n}\n@end\n@fs main_fs\nlayout(location=0) out vec4 color;\nlayout(location=3) out vec4 info;\nvoid main() {\n    color = vec4(1.0);\n    info = vec4(0.0);\n}\n@end\n@program main main_vs main_fs\n");

    EXPECT_EQ(single.programs[0].outputs, 1);
    EXPECT_EQ(several.programs[0].outputs, 4);
}

TEST_F(ReflectionTest, ReportsResourcesWithoutBindings) {
    const std::vector<std::string> found = errors(withFragment("uniform texture2D loose_texture;"));

    EXPECT_EQ(found, std::vector<std::string>{"shader.glsl:7:19: error: Sampler/texture/image requires layout(binding=X) (\"binding\")."});
}

TEST_F(ReflectionTest, ReportsBindingsOutOfRangeOrTaken) {
    const std::vector<std::string> found = errors(R"(@vs main_vs
layout(binding=1) uniform vertex_params { vec4 offset; };
layout(binding=2) uniform texture2D height_texture;
void main() {
    gl_Position = offset;
}
@end
@fs main_fs
layout(binding=8) uniform far { vec4 value_far; };
layout(binding=1) uniform fragment_params { vec4 tint; };
layout(binding=32) uniform texture2D far_texture;
layout(binding=2) uniform texture2D color_texture;
layout(binding=12) uniform sampler far_sampler;
out vec4 frag_color;
void main() {
    frag_color = tint;
}
@end
@program main main_vs main_fs
)");

    const std::vector<std::string> expected = {
        "shader.glsl:9:8: error: The uniform block \"far\" binds to 8, but uniform blocks bind from 0 to 7.", "shader.glsl:11:38: error: The texture \"far_texture\" binds to 32, but textures bind from 0 to 31.", "shader.glsl:13:36: error: The sampler \"far_sampler\" binds to 12, but samplers bind from 0 to 11.", "shader.glsl:10:8: error: The uniform block \"fragment_params\" binds to 1, which the uniform block \"vertex_params\" at line 2 of \"shader.glsl\" already takes.", "shader.glsl:12:37: error: The texture \"color_texture\" binds to 2, which the texture \"height_texture\" at line 3 of \"shader.glsl\" already takes.",
    };
    EXPECT_EQ(found, expected);
}

} // namespace haylen::shaderc::test
