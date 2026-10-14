#include "Parser.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>

namespace serdetk::semver {
namespace {

std::string trim(std::string_view input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = input.find_last_not_of(" \t\r\n");
    return std::string(input.substr(first, last - first + 1));
}

std::vector<std::string> split(std::string_view input, char delimiter) {
    std::vector<std::string> result;
    std::size_t offset = 0;
    while (offset <= input.size()) {
        const auto end = input.find(delimiter, offset);
        result.emplace_back(input.substr(offset, end == std::string_view::npos ? input.size() - offset : end - offset));
        if (end == std::string_view::npos) break;
        offset = end + 1;
    }
    return result;
}

ParseResult<unsigned> number(std::string_view input, std::size_t column) {
    if (input.empty()) return {{}, ParseError{column, "expected numeric version component"}};
    unsigned value = 0;
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
    if (error != std::errc{} || end != input.data() + input.size())
        return {{}, ParseError{column, "invalid numeric version component"}};
    return {value, {}};
}

ParseResult<Version> parse_core(std::string_view input, bool allow_wildcard,
                                std::optional<unsigned>* precision = nullptr) {
    Version result;
    const auto plus = input.find('+');
    const auto dash = input.find('-');
    const auto core_end = std::min(plus == std::string_view::npos ? input.size() : plus,
                                   dash == std::string_view::npos ? input.size() : dash);
    const auto components = split(input.substr(0, core_end), '.');
    if (components.empty() || components.size() > 3)
        return {{}, ParseError{1, "version requires one to three numeric components"}};
    unsigned parsed = 0;
    unsigned values[] = {0, 0, 0};
    for (const auto& component : components) {
        if (allow_wildcard && (component == "*" || component == "x" || component == "X")) break;
        auto value = number(component, 1);
        if (!value) return {{}, value.error};
        values[parsed++] = *value;
    }
    if (precision) *precision = parsed;
    result.major = values[0];
    result.minor = values[1];
    result.patch = values[2];
    if (dash != std::string_view::npos) {
        const auto end = plus == std::string_view::npos ? input.size() : plus;
        const auto identifiers = split(input.substr(dash + 1, end - dash - 1), '.');
        if (identifiers.empty() || std::any_of(identifiers.begin(), identifiers.end(),
            [](const std::string& identifier) { return identifier.empty(); }))
            return {{}, ParseError{dash + 2, "invalid prerelease identifier"}};
        result.prerelease = identifiers;
    }
    if (plus != std::string_view::npos) {
        result.build = std::string(input.substr(plus + 1));
        if (result.build.empty()) return {{}, ParseError{plus + 2, "empty build metadata"}};
    }
    return {std::move(result), {}};
}

Predicate predicate_for(std::string token, std::size_t column) {
    Predicate predicate;
    auto consume = [&](std::string_view prefix, Comparator comparator) {
        if (token.starts_with(prefix)) {
            predicate.comparator = comparator;
            token.erase(0, prefix.size());
            return true;
        }
        return false;
    };
    consume(">=", Comparator::greater_equal) || consume("<=", Comparator::less_equal) ||
        consume(">", Comparator::greater) || consume("<", Comparator::less) ||
        consume("=", Comparator::equal);
    auto parsed = parse_core(token, true, &predicate.precision);
    if (!parsed) throw *parsed.error;
    predicate.version = std::move(*parsed);
    return predicate;
}

} // namespace

ParseResult<Version> parse_version(std::string_view input) {
    const std::string normalized = trim(input);
    if (normalized.empty()) return {{}, ParseError{1, "empty version"}};
    return parse_core(normalized, false);
}

ParseResult<Constraint> parse_constraint(std::string_view input) {
    Constraint result;
    const std::string normalized = trim(input);
    if (normalized.empty() || normalized == "*" || normalized == "x" || normalized == "X")
        return {std::move(result), {}};
    for (const auto& alternative_text : split(normalized, '|')) {
        if (alternative_text.empty()) continue;
        if (alternative_text.front() == '|') continue;
        Clause clause;
        std::istringstream tokens(alternative_text);
        std::string token;
        while (tokens >> token) {
            if (token == ",") continue;
            if (!token.empty() && token.back() == ',') token.pop_back();
            if (token.empty() || token == "*") continue;
            try {
                if (token.front() == '^' || token.front() == '~') {
                    const bool compatible_major = token.front() == '^';
                    auto base = parse_core(std::string_view(token).substr(1), true, nullptr);
                    if (!base) return {{}, base.error};
                    clause.push_back({Comparator::greater_equal, *base, std::nullopt});
                    Version upper = *base;
                    if (compatible_major) {
                        if (upper.major > 0) ++upper.major, upper.minor = upper.patch = 0;
                        else if (upper.minor > 0) ++upper.minor, upper.patch = 0;
                        else ++upper.patch;
                    } else {
                        ++upper.minor;
                        upper.patch = 0;
                    }
                    clause.push_back({Comparator::less, std::move(upper), std::nullopt});
                } else {
                    clause.push_back(predicate_for(token, 1));
                }
            } catch (const ParseError& error) {
                return {{}, error};
            }
        }
        if (clause.empty()) return {{}, ParseError{1, "empty constraint alternative"}};
        result.alternatives.push_back(std::move(clause));
    }
    if (result.alternatives.empty()) return {{}, ParseError{1, "invalid constraint"}};
    return {std::move(result), {}};
}

} // namespace serdetk::semver
