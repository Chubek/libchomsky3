#include "Semver.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace serdetk::semver {
namespace {

int compare_identifier(const std::string& left, const std::string& right) {
    const auto numeric = [](const std::string& value) {
        return !value.empty() && std::all_of(value.begin(), value.end(),
            [](unsigned char character) { return character >= '0' && character <= '9'; });
    };
    const bool left_numeric = numeric(left);
    const bool right_numeric = numeric(right);
    if (left_numeric && right_numeric) {
        unsigned long long left_value = 0;
        unsigned long long right_value = 0;
        std::from_chars(left.data(), left.data() + left.size(), left_value);
        std::from_chars(right.data(), right.data() + right.size(), right_value);
        return left_value < right_value ? -1 : left_value > right_value ? 1 : 0;
    }
    if (left_numeric != right_numeric) return left_numeric ? -1 : 1;
    return left < right ? -1 : left > right ? 1 : 0;
}

} // namespace

std::string Version::str() const {
    std::ostringstream output;
    output << major << '.' << minor << '.' << patch;
    if (!prerelease.empty()) {
        output << '-';
        for (std::size_t index = 0; index < prerelease.size(); ++index) {
            if (index) output << '.';
            output << prerelease[index];
        }
    }
    if (!build.empty()) output << '+' << build;
    return output.str();
}

std::strong_ordering Version::operator<=>(const Version& other) const {
    if (const auto result = major <=> other.major; result != 0) return result;
    if (const auto result = minor <=> other.minor; result != 0) return result;
    if (const auto result = patch <=> other.patch; result != 0) return result;
    if (prerelease.empty() || other.prerelease.empty()) {
        if (prerelease.empty() && other.prerelease.empty()) return std::strong_ordering::equal;
        return prerelease.empty() ? std::strong_ordering::greater : std::strong_ordering::less;
    }
    const auto common = std::min(prerelease.size(), other.prerelease.size());
    for (std::size_t index = 0; index < common; ++index) {
        const int comparison = compare_identifier(prerelease[index], other.prerelease[index]);
        if (comparison < 0) return std::strong_ordering::less;
        if (comparison > 0) return std::strong_ordering::greater;
    }
    return prerelease.size() <=> other.prerelease.size();
}

bool Predicate::matches(const Version& candidate) const {
    if (precision && comparator == Comparator::equal) {
        if (*precision >= 1 && candidate.major != version.major) return false;
        if (*precision >= 2 && candidate.minor != version.minor) return false;
        if (*precision >= 3 && candidate.patch != version.patch) return false;
        return true;
    }
    const auto comparison = candidate <=> version;
    switch (comparator) {
    case Comparator::equal: return comparison == 0;
    case Comparator::greater: return comparison > 0;
    case Comparator::greater_equal: return comparison >= 0;
    case Comparator::less: return comparison < 0;
    case Comparator::less_equal: return comparison <= 0;
    }
    return false;
}

bool Constraint::matches(const Version& candidate) const {
    return alternatives.empty() || std::any_of(alternatives.begin(), alternatives.end(),
        [&candidate](const Clause& clause) {
            return std::all_of(clause.begin(), clause.end(),
                [&candidate](const Predicate& predicate) { return predicate.matches(candidate); });
        });
}

std::string Constraint::str() const {
    return alternatives.empty() ? "*" : "<constraint>";
}

} // namespace serdetk::semver
