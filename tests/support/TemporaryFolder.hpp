#pragma once

#include <filesystem>
#include <string_view>

namespace haylen::shaderc::test {

// A folder of its own under the temporary folder of the system, which goes away with its files when the test ends.
class TemporaryFolder {
  public:
    TemporaryFolder();
    ~TemporaryFolder();
    TemporaryFolder(const TemporaryFolder&) = delete;
    TemporaryFolder& operator=(const TemporaryFolder&) = delete;

    [[nodiscard]] const std::filesystem::path& getPath() const noexcept;

    // Writes a file at a path inside the folder, with the folders it needs, and returns its full path.
    std::filesystem::path write(const std::filesystem::path& relative, std::string_view text) const;

  private:
    std::filesystem::path path;
};

} // namespace haylen::shaderc::test
