#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "haylen/shaderc/PackageWriter.hpp"
#include "support/CompilerFixture.hpp"

namespace haylen::shaderc::test {

class PackageWriterTest : public CompilerFixture {
  protected:
    static constexpr std::string_view kSource = R"(@vs main_vs
layout(binding=0) uniform vertex_params {
    mat4 view_projection;
    float premultiply;
};
in vec2 corner;
out vec2 uv;
void main() {
    uv = corner;
    gl_Position = view_projection * vec4(corner, 0.0, premultiply);
}
@end

@fs main_fs
layout(binding=0) uniform texture2D sprite_texture;
layout(binding=0) uniform sampler sprite_sampler;
#ifdef TINTED
layout(binding=1) uniform fragment_params {
    vec4 tint;
};
#endif
in vec2 uv;
out vec4 frag_color;
void main() {
    frag_color = texture(sampler2D(sprite_texture, sprite_sampler), uv);
#ifdef TINTED
    frag_color *= tint;
#endif
}
@end

@program sprite main_vs main_fs
)";

    static std::string readFile(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }
};

TEST_F(PackageWriterTest, WritesCompactJsonInTheOrderOfTheFormat) {
    const std::string text = PackageWriter::write(compile(kSource, {.variants = {{.name = "plain"}, {.name = "tinted", .defines = {"TINTED"}}}}));

    EXPECT_TRUE(text.starts_with(R"({"format":"haylen-shader","version":2,"name":"shader","sources":[")")) << text.substr(0, 200);
    EXPECT_EQ(text.find('\n'), std::string::npos);

    const nlohmann::ordered_json document = nlohmann::ordered_json::parse(text);
    const nlohmann::ordered_json& program = document.at("programs").at("tinted");
    std::vector<std::string> keys;
    for (const auto& [key, value] : program.items()) {
        keys.push_back(key);
    }
    EXPECT_EQ(keys, (std::vector<std::string>{"attributes", "blocks", "textures", "samplers", "pairs", "outputs", "targets"}));
    EXPECT_EQ(document.at("programs").begin().key(), "plain");
    EXPECT_EQ(program.at("attributes")[0], nlohmann::ordered_json::parse(R"({"location":0,"name":"corner","type":"vec2"})"));
    EXPECT_EQ(program.at("blocks")[0], nlohmann::ordered_json::parse(R"({"name":"vertex_params","stage":"vertex","slot":0,"size":80,"members":[{"name":"view_projection","type":"mat4","count":1,"offset":0},{"name":"premultiply","type":"float","count":1,"offset":64}]})"));
    EXPECT_EQ(program.at("textures")[0], nlohmann::ordered_json::parse(R"({"name":"sprite_texture","stage":"fragment","slot":0,"type":"2d","sampleType":"float"})"));
    EXPECT_EQ(program.at("samplers")[0], nlohmann::ordered_json::parse(R"({"name":"sprite_sampler","stage":"fragment","slot":0,"type":"filtering"})"));
    EXPECT_EQ(program.at("pairs")[0], nlohmann::ordered_json::parse(R"({"stage":"fragment","texture":0,"sampler":0,"name":"sprite_texture_sprite_sampler"})"));
    EXPECT_EQ(program.at("outputs"), 1);

    const nlohmann::ordered_json& targets = program.at("targets");
    std::vector<std::string> names;
    for (const auto& [key, value] : targets.items()) {
        names.push_back(key);
    }
    EXPECT_EQ(names, (std::vector<std::string>{"msl-macos", "msl-ios", "msl-ios-simulator", "hlsl5", "glsl430", "glsl300es", "wgsl", "spirv"}));
    EXPECT_EQ(targets.at("msl-macos").at("blocks"), nlohmann::ordered_json::parse(R"({"0":0,"1":0})"));
    EXPECT_EQ(targets.at("hlsl5").at("vertex").at("profile"), "vs_5_0");
    EXPECT_FALSE(targets.at("wgsl").at("vertex").contains("profile"));
    EXPECT_EQ(targets.at("glsl430").at("blocks"), nlohmann::ordered_json::parse(R"({"0":{"name":"vertex_params","type":"vec4","count":5},"1":{"name":"fragment_params","type":"vec4","count":1}})"));
    EXPECT_FALSE(targets.at("glsl430").contains("textures"));
    EXPECT_EQ(targets.at("wgsl").at("samplers"), nlohmann::ordered_json::parse(R"({"0":64})"));
    EXPECT_EQ(targets.at("spirv").at("vertex").at("entry"), "main");
    EXPECT_EQ(targets.at("spirv").at("blocks"), nlohmann::ordered_json::parse(R"({"0":{"set":0,"binding":0},"1":{"set":0,"binding":1}})"));
    EXPECT_EQ(targets.at("spirv").at("textures"), nlohmann::ordered_json::parse(R"({"0":{"set":1,"binding":0}})"));
    EXPECT_EQ(targets.at("spirv").at("samplers"), nlohmann::ordered_json::parse(R"({"0":{"set":1,"binding":64}})"));
}

TEST_F(PackageWriterTest, ListsEveryDistinctSourceOnceInTheOrderOfFirstUse) {
    const Package package = compile(kSource, {.variants = {{.name = "plain"}, {.name = "tinted", .defines = {"TINTED"}}}});

    const std::set<std::string> distinct(package.sources.begin(), package.sources.end());
    EXPECT_EQ(distinct.size(), package.sources.size());

    int next = 0;
    for (const Package::Program& program : package.programs) {
        for (const Package::Code& code : program.targets) {
            for (const int source : {code.vertex.source, code.fragment.source}) {
                EXPECT_LE(source, next);
                next = std::max(next, source + 1);
            }
        }
    }
    EXPECT_EQ(next, static_cast<int>(package.sources.size()));

    // The variants share their vertex stage, which compiles to the same source for each target.
    EXPECT_EQ(package.programs[0].targets[0].vertex.source, package.programs[1].targets[0].vertex.source);
    EXPECT_NE(package.programs[0].targets[0].fragment.source, package.programs[1].targets[0].fragment.source);
}

TEST_F(PackageWriterTest, WritesTheSameBytesForTheSameSource) {
    const Compiler::Options variants{.variants = {{.name = "plain"}, {.name = "tinted", .defines = {"TINTED"}}}};
    Compiler::Options single = variants;
    single.threads = 1;
    Compiler::Options many = variants;
    many.threads = 8;

    const std::string first = PackageWriter::write(compile(kSource, single));
    EXPECT_EQ(PackageWriter::write(compile(kSource, many)), first);
    EXPECT_EQ(PackageWriter::write(compile(kSource, variants)), first);
}

TEST_F(PackageWriterTest, SavesThroughATemporaryFileThatReplacesThePackage) {
    const std::filesystem::path path = write("out/sprite.shader", "old contents");
    const Package package = compile(kSource);

    PackageWriter::save(package, path);

    EXPECT_EQ(readFile(path), PackageWriter::write(package));
    std::vector<std::filesystem::path> files;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path.parent_path())) {
        files.push_back(entry.path().filename());
    }
    EXPECT_EQ(files, std::vector<std::filesystem::path>{"sprite.shader"});
}

TEST_F(PackageWriterTest, ReportsPackagesThatCannotBeWritten) {
    const Package package = compile(kSource);

    EXPECT_THROW(PackageWriter::save(package, folder.getPath() / "missing" / "sprite.shader"), std::runtime_error);
}

} // namespace haylen::shaderc::test
