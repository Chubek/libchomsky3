#pragma once

#include "Evalutator.hpp"
#include "Parser.hpp"
#include "SerdeTk-Plugin.hpp"
#include "Validator.hpp"

namespace serdetk::xpath {

class Plugin final : public serdetk::plugin::Plugin {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "serdetk.xpath"; }
    [[nodiscard]] std::string_view version() const noexcept override { return "3.1"; }
    [[nodiscard]] Sequence query(const Document& document, std::string_view expression) const;
    [[nodiscard]] Diagnostics validate_expression(std::string_view expression) const;

private:
    SerdeTkAdapter adapter_ {};
    Parser parser_ {};
    Validator validator_ {};
};

} // namespace serdetk::xpath
