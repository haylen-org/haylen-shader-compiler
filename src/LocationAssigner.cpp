#include "LocationAssigner.hpp"

#include <algorithm>
#include <format>
#include <set>

namespace haylen::shaderc {

void LocationAssigner::assign(Declarations& vertex, Declarations& fragment, const ShaderSource& source, std::vector<Diagnostic>& diagnostics) {
    assignInOrder(vertex.inputs);
    assignInOrder(vertex.outputs);
    assignInOrder(fragment.outputs);

    for (Declarations::Variable& input : fragment.inputs) {
        const auto output = std::ranges::find(vertex.outputs, input.name, &Declarations::Variable::name);
        if (output == vertex.outputs.end()) {
            diagnostics.push_back(source.diagnose(input.location, std::format("The fragment input \"{}\" has no vertex output with the same name.", input.name)));
            continue;
        }
        if (output->type != input.type) {
            diagnostics.push_back(source.diagnose(input.location, std::format("The fragment input \"{}\" is a \"{}\", but the vertex output with the same name is a \"{}\".", input.name, input.type, output->type)));
            continue;
        }
        if (input.explicitLocation && *input.explicitLocation != output->assignedLocation) {
            diagnostics.push_back(source.diagnose(input.location, std::format("The fragment input \"{}\" is at location {}, but the vertex output with the same name is at location {}.", input.name, *input.explicitLocation, output->assignedLocation)));
            continue;
        }
        input.assignedLocation = output->assignedLocation;
    }
}

void LocationAssigner::assignInOrder(std::vector<Declarations::Variable>& variables) {
    std::set<int> taken;
    for (Declarations::Variable& variable : variables) {
        if (variable.explicitLocation) {
            variable.assignedLocation = *variable.explicitLocation;
            for (int offset = 0; offset < variable.locationSize; ++offset) {
                taken.insert(*variable.explicitLocation + offset);
            }
        }
    }

    // Each variable without a location takes the first free run of locations after the one before it.
    int next = 0;
    for (Declarations::Variable& variable : variables) {
        if (variable.explicitLocation) {
            continue;
        }
        // clang-format off
        const auto overlaps = [&taken, &variable](int start) {
            for (int offset = 0; offset < variable.locationSize; ++offset) {
                if (taken.contains(start + offset)) {
                    return true;
                }
            }
            return false;
        };
        // clang-format on
        while (overlaps(next)) {
            ++next;
        }
        variable.assignedLocation = next;
        for (int offset = 0; offset < variable.locationSize; ++offset) {
            taken.insert(next + offset);
        }
        next += variable.locationSize;
    }
}

} // namespace haylen::shaderc
