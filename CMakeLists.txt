cmake_minimum_required(VERSION 3.28...4.4)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" HAYLEN_SHADERC_VERSION)
string(STRIP "${HAYLEN_SHADERC_VERSION}" HAYLEN_SHADERC_VERSION)

project(haylen-shader-compiler VERSION ${HAYLEN_SHADERC_VERSION} LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HAYLEN_SHADERC_BUILD_TESTS "Build the tests of the shader compiler." ${PROJECT_IS_TOP_LEVEL})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(cpm)
include(haylen-shaderc-warnings)
include(haylen-shaderc-dependencies)

find_package(Threads REQUIRED)

add_library(haylen-shaderc-library STATIC
  src/CommandLine.cpp
  src/CompileError.cpp
  src/Compiler.cpp
  src/DeclarationChecker.cpp
  src/DeclarationReader.cpp
  src/Diagnostic.cpp
  src/GlslTranslator.cpp
  src/GlslangLog.cpp
  src/HlslTranslator.cpp
  src/LocationAssigner.cpp
  src/MetalTranslator.cpp
  src/Names.cpp
  src/Package.cpp
  src/PackageWriter.cpp
  src/PathText.cpp
  src/Preprocessor.cpp
  src/Reflector.cpp
  src/ShaderSource.cpp
  src/SourceFile.cpp
  src/SpirvPatcher.cpp
  src/SpirvTranslator.cpp
  src/StageCompiler.cpp
  src/TargetBindings.cpp
  src/ThreadPool.cpp
  src/Translation.cpp
  src/WgslTranslator.cpp
)
add_library(haylen::shaderc ALIAS haylen-shaderc-library)
target_include_directories(haylen-shaderc-library PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include" PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
# The headers of Tint include the utilities of Dawn, which read the definitions of their build configuration.
target_link_libraries(haylen-shaderc-library PRIVATE glslang::glslang glslang::glslang-default-resource-limits spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl tint_api dawn_utils_config nlohmann_json::nlohmann_json Threads::Threads)
target_compile_definitions(haylen-shaderc-library PRIVATE HAYLEN_SHADERC_VERSION="${PROJECT_VERSION}")
haylen_shaderc_enable_warnings(haylen-shaderc-library)

add_executable(haylen-shaderc tools/Main.cpp)
target_link_libraries(haylen-shaderc PRIVATE haylen::shaderc)
haylen_shaderc_enable_warnings(haylen-shaderc)

if(HAYLEN_SHADERC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
