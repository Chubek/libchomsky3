#include "Hooks.hpp"

namespace serdetk::xpath {

Sequence Plugin::query(const Document& document, std::string_view expression) const {
    auto tree = parser_.parse(expression);
    auto diagnostics = validator_.validate(*tree);
    if (diagnostics.has_errors()) throw QueryError(diagnostics.items.front().message);
    return Evalutator(adapter_).evaluate(CodeDAG::compile(std::move(tree)), document);
}

Diagnostics Plugin::validate_expression(std::string_view expression) const {
    try {
        auto tree = parser_.parse(expression);
        return validator_.validate(*tree);
    } catch (const Error& error) {
        Diagnostics diagnostics;
        diagnostics.add({Diagnostic::Severity::Error, error.what(), {}});
        return diagnostics;
    }
}

} // namespace serdetk::xpath
