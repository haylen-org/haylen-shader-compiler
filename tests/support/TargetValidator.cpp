#include "support/TargetValidator.hpp"

#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

#include <glslang/Include/PoolAlloc.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <spirv-tools/libspirv.hpp>
#include <src/tint/lang/wgsl/reader/reader.h>

#include "support/SpirvModule.hpp"
#include "support/TemporaryFolder.hpp"

namespace haylen::shaderc::test {

std::string TargetValidator::validateGlsl(const std::string& vertex, const std::string& fragment) {
    // The sources state their own version and profile, and linking them checks that the outputs of the vertex stage feed the inputs of the fragment stage.
    glslang::InitializeProcess();
    std::string log;
    {
        glslang::TShader vertexShader(EShLangVertex);
        glslang::TShader fragmentShader(EShLangFragment);
        glslang::TProgram program;
        const char* vertexText = vertex.c_str();
        const char* fragmentText = fragment.c_str();
        vertexShader.setStrings(&vertexText, 1);
        fragmentShader.setStrings(&fragmentText, 1);
        for (glslang::TShader* shader : {&vertexShader, &fragmentShader}) {
            if (!shader->parse(GetDefaultResources(), 100, false, EShMsgDefault)) {
                log += shader->getInfoLog();
            }
            program.addShader(shader);
        }
        if (log.empty() && !program.link(EShMsgDefault)) {
            log += program.getInfoLog();
        }
    }

    // The shaders and the program leave the thread on their pools of memory, which are gone now.
    glslang::SetThreadPoolAllocator(nullptr);
    glslang::FinalizeProcess();
    return log;
}

std::string TargetValidator::validateWgsl(const std::string& source) {
    const tint::Source::File file("shader.wgsl", source);
    const tint::Program program = tint::wgsl::reader::Parse(&file);
    return program.IsValid() ? std::string() : program.Diagnostics().Str();
}

std::string TargetValidator::validateSpirv(const std::string& source) {
    const std::optional<std::vector<std::uint32_t>> words = SpirvModule::decode(source);
    if (!words) {
        return "The source is not base64 text of whole SPIR-V words.";
    }
    std::string log;
    spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_1);
    // clang-format off
    tools.SetMessageConsumer([&log](spv_message_level_t, const char*, const spv_position_t& position, const char* message) {
        log += std::format("Word {}: {}\n", position.index, message);
    });
    // clang-format on
    return tools.Validate(*words) ? std::string() : log;
}

bool TargetValidator::canValidateMetal() {
    // A shader that every Metal compiler accepts tells a missing toolchain from an invalid source.
    static const bool installed = compileMetal({"#include <metal_stdlib>\nusing namespace metal;\nfragment float4 probe() { return float4(1.0); }\n"}, Package::Target::MslMacos).empty();
    return installed;
}

std::string TargetValidator::validateMetal(const std::vector<std::string>& sources, Package::Target target) {
    return compileMetal(sources, target);
}

std::string TargetValidator::compileMetal(const std::vector<std::string>& sources, Package::Target target) {
#ifdef __APPLE__
    const TemporaryFolder folder;
    const std::string sdk = target == Package::Target::MslMacos ? "macosx" : target == Package::Target::MslIos ? "iphoneos" : "iphonesimulator";
    const std::string standard = target == Package::Target::MslMacos ? "macos-metal2.0" : "ios-metal2.0";
    std::string files;
    for (std::size_t index = 0; index < sources.size(); ++index) {
        const std::string name = std::format("stage{}.metal", index);
        folder.write(name, sources[index]);
        files += " " + name;
    }
    std::string output;
    if (runCommand(std::format("cd \"{}\" && xcrun -sdk {} metal -std={} -c{}", folder.getPath().string(), sdk, standard, files), output) == 0) {
        return {};
    }
    return output.empty() ? std::string("The Metal compiler failed without a message.") : output;
#else
    static_cast<void>(sources);
    static_cast<void>(target);
    return "The Metal compiler runs only on macOS.";
#endif
}

bool TargetValidator::canValidateHlsl() {
#ifdef _WIN32
    // clang-format off
    static const bool installed = [] {
        std::string output;
        return runCommand("where fxc", output) == 0;
    }();
    // clang-format on
    return installed;
#else
    return false;
#endif
}

std::string TargetValidator::validateHlsl(const std::string& source, const std::string& profile, const std::string& entry) {
    const TemporaryFolder folder;
    const std::filesystem::path file = folder.write("stage.hlsl", source);
    std::string output;
    if (runCommand(std::format("fxc /nologo /T {} /E {} /Fo \"{}\" \"{}\"", profile, entry, (folder.getPath() / "stage.cso").string(), file.string()), output) == 0) {
        return {};
    }
    return output.empty() ? std::string("The HLSL compiler failed without a message.") : output;
}

int TargetValidator::runCommand(const std::string& command, std::string& output) {
    const TemporaryFolder folder;
    const std::filesystem::path log = folder.getPath() / "output.txt";
    const int status = std::system(std::format("{} > \"{}\" 2>&1", command, log.string()).c_str());
    std::ifstream stream(log, std::ios::binary);
    output.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return status;
}

} // namespace haylen::shaderc::test
