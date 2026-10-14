#include "Storage.hpp"

#include <algorithm>

namespace serdetk::semver {

bool Storage::add(std::string package, PackageVersion release) {
    if (package.empty()) return false;
    auto& versions = releases_[std::move(package)];
    if (std::any_of(versions.begin(), versions.end(), [&release](const PackageVersion& existing) {
        return existing.version == release.version;
    })) return false;
    versions.push_back(std::move(release));
    std::sort(versions.begin(), versions.end(), [](const PackageVersion& left, const PackageVersion& right) {
        return left.version > right.version;
    });
    return true;
}

const std::vector<PackageVersion>* Storage::find(std::string_view package) const {
    const auto iterator = releases_.find(package);
    return iterator == releases_.end() ? nullptr : &iterator->second;
}

std::vector<std::string> Storage::packages() const {
    std::vector<std::string> result;
    result.reserve(releases_.size());
    for (const auto& [package, _] : releases_) result.push_back(package);
    return result;
}

} // namespace serdetk::semver
