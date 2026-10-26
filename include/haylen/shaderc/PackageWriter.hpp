#pragma once

#include <filesystem>
#include <string>

#include "haylen/shaderc/Package.hpp"

namespace haylen::shaderc {

// Writes packages as the compact JSON of `docs/format.md`, the same bytes for the same package.
class PackageWriter {
  public:
    [[nodiscard]] static std::string write(const Package& package);

    // Writes a temporary file next to the path and renames it over the path, so readers never see a partial package. Throws `std::runtime_error`.
    static void save(const Package& package, const std::filesystem::path& path);

  private:
    [[nodiscard]] static std::string temporarySuffix();
};

} // namespace haylen::shaderc
