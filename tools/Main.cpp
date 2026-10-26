#include <iostream>
#include <string_view>
#include <vector>

#include "haylen/shaderc/CommandLine.hpp"

int main(int count, char** values) {
    const std::vector<std::string_view> arguments(values + 1, values + count);
    return haylen::shaderc::CommandLine::run(arguments, std::cout, std::cerr);
}
