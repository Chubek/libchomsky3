/** @file DSLtk.hpp
 *  @brief Header-only parser combinator toolkit (placeholder skeleton).
 */
#ifndef JITERATI_DSLTK_HPP_INCLUDED
#define JITERATI_DSLTK_HPP_INCLUDED

#include <functional>
#include <optional>
#include <string_view>

namespace jiterati::dsltk {

struct ParseError {
    std::size_t line = 1;
    std::size_t column = 1;
    const char* message = "";
};

template <typename T>
struct ParseResult {
    T value;
    std::string_view rest;
    ParseError error;
};

template <typename T>
using Parser = std::function<std::optional<ParseResult<T>>(std::string_view)>;

} // namespace jiterati::dsltk

#endif // JITERATI_DSLTK_HPP_INCLUDED
