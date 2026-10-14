#pragma once

#include "DepndencyGraph.hpp"
#include "Storage.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace serdetk::semver {

struct Diagnostic {
    std::string package;
    std::string message;
};

struct Resolution {
    bool satisfiable = false;
    std::map<std::string, PackageVersion, std::less<>> selected;
    DependencyGraph graph;
    std::vector<Diagnostic> diagnostics;
};

class Resolver {
public:
    explicit Resolver(const Storage& storage) : storage_(storage) {}

    [[nodiscard]] Resolution resolve(
        const std::map<std::string, Constraint, std::less<>>& requirements) const;

private:
    const Storage& storage_;
};

} // namespace serdetk::semver
