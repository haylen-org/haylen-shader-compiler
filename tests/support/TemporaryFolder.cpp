#include "support/TemporaryFolder.hpp"

#include <format>
#include <fstream>
#include <random>
#include <system_error>

namespace haylen::shaderc::test {

TemporaryFolder::TemporaryFolder() {
    std::random_device device;
    path = std::filesystem::temp_directory_path() / std::format("haylen-shaderc-{:08x}{:08x}", device(), device());
    std::filesystem::create_directories(path);
}

TemporaryFolder::~TemporaryFolder() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
}

const std::filesystem::path& TemporaryFolder::getPath() const noexcept {
    return path;
}

std::filesystem::path TemporaryFolder::write(const std::filesystem::path& relative, std::string_view text) const {
    const std::filesystem::path file = path / relative;
    std::filesystem::create_directories(file.parent_path());
    std::ofstream stream(file, std::ios::binary);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return file;
}

} // namespace haylen::shaderc::test
