#pragma once

#include <string>
#include <vector>

#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc::test {

// Checks the sources of the targets with real compilers: glslang for GLSL, the WGSL reader of Tint, the SPIR-V validator of SPIRV-Tools for Vulkan 1.1, and the Metal and HLSL compilers of the system where they are installed. Every check returns the errors of the compiler, empty when the sources are valid.
class TargetValidator {
  public:
    [[nodiscard]] static std::string validateGlsl(const std::string& vertex, const std::string& fragment);
    [[nodiscard]] static std::string validateWgsl(const std::string& source);
    [[nodiscard]] static std::string validateSpirv(const std::string& source);

    // Tells whether the Metal toolchain of Xcode is installed, which compiles every Metal target.
    [[nodiscard]] static bool canValidateMetal();
    [[nodiscard]] static std::string validateMetal(const std::vector<std::string>& sources, Package::Target target);

    // Tells whether the HLSL compiler "fxc" of the Windows SDK is on the path.
    [[nodiscard]] static bool canValidateHlsl();
    [[nodiscard]] static std::string validateHlsl(const std::string& source, const std::string& profile, const std::string& entry);

  private:
    [[nodiscard]] static std::string compileMetal(const std::vector<std::string>& sources, Package::Target target);
    [[nodiscard]] static int runCommand(const std::string& command, std::string& output);
};

} // namespace haylen::shaderc::test
