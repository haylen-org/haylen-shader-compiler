#pragma once

#include <vector>

#include "Declarations.hpp"
#include "ShaderSource.hpp"
#include "haylen/shaderc/Diagnostic.hpp"

namespace haylen::shaderc {

// Gives every input and output of a program its location. The vertex inputs, the vertex outputs and the fragment outputs take the first free locations in the order the source declares them, and every fragment input takes the location of the vertex output with the same name.
class LocationAssigner {
  public:
    static void assign(Declarations& vertex, Declarations& fragment, const ShaderSource& source, std::vector<Diagnostic>& diagnostics);

  private:
    static void assignInOrder(std::vector<Declarations::Variable>& variables);
};

} // namespace haylen::shaderc
