#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "haylen/shaderc/CommandLine.hpp"
#include "support/TemporaryFolder.hpp"

namespace haylen::shaderc::test {

class CommandLineTest : public ::testing::Test {
  protected:
    static constexpr std::string_view kSource = R"(@vs main_vs
void main() {
    gl_Position = vec4(0.0);
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

    TemporaryFolder folder;
    std::ostringstream out;
    std::ostringstream err;

    int run(const std::vector<std::string>& arguments) {
        const std::vector<std::string_view> views(arguments.begin(), arguments.end());
        return CommandLine::run(views, out, err);
    }

    std::string path(std::string_view name) const {
        return (folder.getPath() / name).string();
    }
};

TEST_F(CommandLineTest, ShowsTheUsage) {
    EXPECT_EQ(run({"--help"}), CommandLine::kSuccess);
    EXPECT_TRUE(out.str().starts_with("Usage: haylen-shaderc <input.glsl> --output <file.shader> [options]")) << out.str();
    EXPECT_NE(out.str().find("\"msl-ios-simulator\""), std::string::npos);
}

TEST_F(CommandLineTest, ShowsTheVersion) {
    EXPECT_EQ(run({"--version"}), CommandLine::kSuccess);
    EXPECT_EQ(out.str(), std::string("haylen-shaderc ") + HAYLEN_SHADERC_VERSION + "\n");
}

TEST_F(CommandLineTest, WritesThePackage) {
    const std::string input = folder.write("main.glsl", kSource).string();

    EXPECT_EQ(run({input, "--output", path("main.shader"), "--target", "wgsl", "--target", "glsl430"}), CommandLine::kSuccess) << err.str();
    EXPECT_EQ(err.str(), "");
    EXPECT_EQ(out.str(), "");
    std::ifstream stream(path("main.shader"));
    const nlohmann::json document = nlohmann::json::parse(stream);
    EXPECT_EQ(document.at("name"), "main");
    EXPECT_EQ(document.at("programs").at("main").at("targets").size(), 2U);
}

TEST_F(CommandLineTest, NamesProgramsAfterVariants) {
    const std::string input = folder.write("main.glsl", kSource).string();

    EXPECT_EQ(run({input, "--output", path("main.shader"), "--variant", "plain=", "--variant", "lit=HAYLEN_LIT,HAYLEN_BATCH", "--variant", "bare"}), CommandLine::kSuccess) << err.str();
    std::ifstream stream(path("main.shader"));
    const nlohmann::ordered_json document = nlohmann::ordered_json::parse(stream);
    std::vector<std::string> names;
    for (const auto& [key, value] : document.at("programs").items()) {
        names.push_back(key);
    }
    EXPECT_EQ(names, (std::vector<std::string>{"plain", "lit", "bare"}));
}

TEST_F(CommandLineTest, ReportsCompileErrors) {
    const std::string input = folder.write("main.glsl", "@vs main_vs\n@end\n@fs main_fs\n@end\n@program main main_vs other_fs\n").string();

    EXPECT_EQ(run({input, "--output", path("main.shader")}), CommandLine::kCompileFailed);
    EXPECT_EQ(err.str(), std::filesystem::path(input).lexically_normal().generic_string() + ":5:23: error: There is no stage named \"other_fs\".\n");
    EXPECT_FALSE(std::filesystem::exists(path("main.shader")));
}

TEST_F(CommandLineTest, ReportsVariantsOfFilesWithManyPrograms) {
    const std::string input = folder.write("main.glsl", std::string(kSource) + "@program other main_vs main_fs\n").string();

    EXPECT_EQ(run({input, "--output", path("main.shader"), "--variant", "lit=LIT"}), CommandLine::kCompileFailed);
    EXPECT_NE(err.str().find("Variants compile the one program of a file, but this file declares 2 programs."), std::string::npos) << err.str();
}

TEST_F(CommandLineTest, ReportsInputsThatCannotBeRead) {
    EXPECT_EQ(run({path("missing.glsl"), "--output", path("main.shader")}), CommandLine::kCompileFailed);
    EXPECT_EQ(err.str(), std::filesystem::path(path("missing.glsl")).generic_string() + ": error: The file cannot be read.\n");
}

TEST_F(CommandLineTest, RejectsWrongArguments) {
    const std::string input = folder.write("main.glsl", kSource).string();
    const std::string output = path("main.shader");
    const std::vector<std::pair<std::vector<std::string>, std::string>> cases = {
        {{"--output", output}, "The command needs an input file, such as \"haylen-shaderc sprite.glsl --output sprite.shader\"."}, {{input}, "The command needs the package to write, which \"--output\" names."}, {{input, "--output"}, "The option \"--output\" needs a value after it."}, {{input, "--output", output, "--output", output}, "The option \"--output\" is given twice."}, {{input, input, "--output", output}, std::format("The command takes one input file, but it is given \"{}\" and \"{}\".", std::filesystem::path(input).generic_string(), input)}, {{input, "--output", output, "--optimize"}, "The option \"--optimize\" does not exist."}, {{input, "--output", output, "--target", "hlsl6"}, "The target \"hlsl6\" does not exist. The targets are \"msl-macos\", \"msl-ios\", \"msl-ios-simulator\", \"hlsl5\", \"glsl430\", \"glsl300es\", \"wgsl\" and \"spirv\"."}, {{input, "--output", output, "--target", "wgsl", "--target", "wgsl"}, "The target \"wgsl\" is given twice."}, {{input, "--output", output, "--variant", "lit=A,,B"}, "The variant \"lit\" has an empty define. Separate its defines with single commas, such as \"--variant lit=HAYLEN_LIT,HAYLEN_BATCH\"."}, {{input, "--output", output, "--variant", "=A"}, "The variant name \"\" is not made of letters, digits, underscores and dashes."}, {{input, "--output", output, "--variant", "lit=A", "--variant", "lit=B"}, "The variant \"lit\" is given twice."}, {{input, "--output", output, "--variant", "lit=2D"}, "The define \"2D\" of the variant \"lit\" is not an identifier."},
    };

    for (const auto& [arguments, message] : cases) {
        err.str("");
        EXPECT_EQ(run(arguments), CommandLine::kBadArguments) << message;
        EXPECT_EQ(err.str(), "haylen-shaderc: error: " + message + "\nRun \"haylen-shaderc --help\" to see how to use it.\n");
    }
    EXPECT_FALSE(std::filesystem::exists(output));
}

} // namespace haylen::shaderc::test
