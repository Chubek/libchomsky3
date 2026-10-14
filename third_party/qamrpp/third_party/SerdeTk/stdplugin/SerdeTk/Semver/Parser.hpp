#pragma once

#include "Semver.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace serdetk::semver {

struct ParseError {
    std::size_t column = 1;
    std::string message;
};

template <typename Value>
struct ParseResult {
    std::optional<Value> value;
    std::optional<ParseError> error;

    [[nodiscard]] explicit operator bool() const noexcept { return value.has_value(); }
    [[nodiscard]] Value& operator*() { return *value; }
    [[nodiscard]] const Value& operator*() const { return *value; }
    [[nodiscard]] Value* operator->() { return &*value; }
    [[nodiscard]] const Value* operator->() const { return &*value; }
};

[[nodiscard]] ParseResult<Version> parse_version(std::string_view input);
[[nodiscard]] ParseResult<Constraint> parse_constraint(std::string_view input);

} // namespace serdetk::semver
