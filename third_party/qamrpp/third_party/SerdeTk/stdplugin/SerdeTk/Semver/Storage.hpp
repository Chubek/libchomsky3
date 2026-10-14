#pragma once

#include "Semver.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace serdetk::semver {

struct PackageVersion {
    Version version;
    std::map<std::string, Constraint, std::less<>> dependencies;
};

class Storage {
public:
    bool add(std::string package, PackageVersion release);
    [[nodiscard]] const std::vector<PackageVersion>* find(std::string_view package) const;
    [[nodiscard]] std::vector<std::string> packages() const;

private:
    std::map<std::string, std::vector<PackageVersion>, std::less<>> releases_;
};

} // namespace serdetk::semver
