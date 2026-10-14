#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace serdetk::semver {

struct Version {
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
    std::vector<std::string> prerelease;
    std::string build;

    [[nodiscard]] std::string str() const;
    [[nodiscard]] bool is_prerelease() const noexcept { return !prerelease.empty(); }
    std::strong_ordering operator<=>(const Version&) const;
    bool operator==(const Version&) const = default;
};

enum class Comparator {
    equal,
    greater,
    greater_equal,
    less,
    less_equal,
};

struct Predicate {
    Comparator comparator = Comparator::equal;
    Version version;
    std::optional<unsigned> precision;

    [[nodiscard]] bool matches(const Version& candidate) const;
};

using Clause = std::vector<Predicate>;

struct Constraint {
    std::vector<Clause> alternatives;

    [[nodiscard]] bool matches(const Version& candidate) const;
    [[nodiscard]] bool empty() const noexcept { return alternatives.empty(); }
    [[nodiscard]] std::string str() const;
};

} // namespace serdetk::semver
