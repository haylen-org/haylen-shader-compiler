# The package writer writes its JSON with nlohmann/json.
CPMAddPackage(
  NAME nlohmann_json
  VERSION 3.12.0
  URL "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz"
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  OPTIONS "JSON_BuildTests OFF" "JSON_Install OFF"
  SYSTEM YES
)

# The glslang library compiles the stages of the shader language into SPIR-V. The compiler runs no SPIR-V optimizer, so glslang builds without SPIRV-Tools, its HLSL front end and its tools.
CPMAddPackage(
  NAME glslang
  URL "https://github.com/KhronosGroup/glslang/archive/refs/tags/16.6.0.tar.gz"
  URL_HASH SHA256=9c09b901149c729df745057dafa815278aaa101b84d2b6e14f16a42de52f97f2
  OPTIONS "ENABLE_OPT OFF" "ENABLE_HLSL OFF" "ENABLE_GLSLANG_BINARIES OFF" "ENABLE_GLSLANG_JS OFF" "GLSLANG_TESTS OFF" "GLSLANG_ENABLE_INSTALL OFF" "BUILD_EXTERNAL OFF" "ENABLE_PCH OFF"
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)
haylen_shaderc_disable_folder_warnings("${glslang_SOURCE_DIR}")

# SPIRV-Cross reflects the SPIR-V of every stage and translates it into the Metal Shading Language, HLSL and GLSL.
CPMAddPackage(
  NAME spirv_cross
  URL "https://github.com/KhronosGroup/SPIRV-Cross/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz"
  URL_HASH SHA256=f708982e88b763ef5b0394ed468fc2c3628f68cdab89f2e7a946cd628c04e721
  OPTIONS "SPIRV_CROSS_ENABLE_TESTS OFF" "SPIRV_CROSS_CLI OFF" "SPIRV_CROSS_SHARED OFF" "SPIRV_CROSS_STATIC ON" "SPIRV_CROSS_ENABLE_GLSL ON" "SPIRV_CROSS_ENABLE_HLSL ON" "SPIRV_CROSS_ENABLE_MSL ON" "SPIRV_CROSS_ENABLE_REFLECT ON" "SPIRV_CROSS_ENABLE_CPP OFF" "SPIRV_CROSS_ENABLE_C_API OFF" "SPIRV_CROSS_ENABLE_UTIL OFF" "SPIRV_CROSS_SKIP_INSTALL ON"
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)
haylen_shaderc_disable_folder_warnings("${spirv_cross_SOURCE_DIR}")

# Tint of the Dawn project reads SPIR-V and writes WGSL. Dawn publishes no releases, so it is pinned to a commit of its default branch, and its own script fetches the sources it depends on. Only the library of Tint builds, with the SPIR-V reader and the WGSL reader and writer, while every backend, sample, test and tool of Dawn stays off.
CPMAddPackage(
  NAME dawn
  URL "https://github.com/google/dawn/archive/43b805ec9edc136b42b55f77ff46f90e73ca2a07.tar.gz"
  URL_HASH SHA256=32b615dbb150f855c0ec0b51410653a025fd6654a8845f4251a5dc03a996ec10
  OPTIONS
    "DAWN_FETCH_DEPENDENCIES ON"
    "DAWN_BUILD_SAMPLES OFF"
    "DAWN_BUILD_TESTS OFF"
    "DAWN_BUILD_PROTOBUF OFF"
    "DAWN_BUILD_NODE_BINDINGS OFF"
    "DAWN_ENABLE_METAL OFF"
    "DAWN_ENABLE_VULKAN OFF"
    "DAWN_ENABLE_NULL OFF"
    "DAWN_ENABLE_DESKTOP_GL OFF"
    "DAWN_ENABLE_OPENGLES OFF"
    "DAWN_ENABLE_D3D11 OFF"
    "DAWN_ENABLE_D3D12 OFF"
    "DAWN_USE_GLFW OFF"
    "DAWN_USE_X11 OFF"
    "DAWN_USE_WAYLAND OFF"
    "TINT_BUILD_TESTS OFF"
    "TINT_BUILD_CMD_TOOLS OFF"
    "TINT_BUILD_SPV_READER ON"
    "TINT_BUILD_WGSL_READER ON"
    "TINT_BUILD_WGSL_WRITER ON"
    "TINT_BUILD_GLSL_WRITER OFF"
    "TINT_BUILD_GLSL_VALIDATOR OFF"
    "TINT_BUILD_HLSL_WRITER OFF"
    "TINT_BUILD_MSL_WRITER OFF"
    "TINT_BUILD_SPV_WRITER OFF"
    "TINT_BUILD_NULL_WRITER OFF"
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)
haylen_shaderc_disable_folder_warnings("${dawn_SOURCE_DIR}")

if(HAYLEN_SHADERC_BUILD_TESTS)
  CPMAddPackage(
    NAME googletest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz"
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    OPTIONS "BUILD_GMOCK OFF" "INSTALL_GTEST OFF" "gtest_force_shared_crt ON"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
  )
  haylen_shaderc_disable_folder_warnings("${googletest_SOURCE_DIR}")
endif()
