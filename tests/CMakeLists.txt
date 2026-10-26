include(GoogleTest)

add_executable(haylen-shaderc-tests
  CommandLineTest.cpp
  HaylenShadersTest.cpp
  LanguageTest.cpp
  PackageWriterTest.cpp
  ReflectionTest.cpp
  SourceFileTest.cpp
  TargetTest.cpp
  support/CompilerFixture.cpp
  support/SpirvModule.cpp
  support/TargetValidator.cpp
  support/TemporaryFolder.cpp
)
target_include_directories(haylen-shaderc-tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${PROJECT_SOURCE_DIR}/src")
# The tests check the GLSL targets with glslang, the WGSL target with the WGSL reader of Tint and the SPIR-V target with the validator of SPIRV-Tools, which Tint builds for its SPIR-V reader.
target_link_libraries(haylen-shaderc-tests PRIVATE haylen::shaderc glslang::glslang glslang::glslang-default-resource-limits tint_api dawn_utils_config SPIRV-Tools nlohmann_json::nlohmann_json GTest::gtest_main)
target_compile_definitions(haylen-shaderc-tests PRIVATE HAYLEN_SHADERC_TEST_DATA="${CMAKE_CURRENT_LIST_DIR}/data" HAYLEN_SHADERC_VERSION="${PROJECT_VERSION}")
haylen_shaderc_enable_warnings(haylen-shaderc-tests)
gtest_discover_tests(haylen-shaderc-tests DISCOVERY_MODE PRE_TEST)
