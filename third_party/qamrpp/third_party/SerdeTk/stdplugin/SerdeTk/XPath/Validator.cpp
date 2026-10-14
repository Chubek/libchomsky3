#include "Validator.hpp"

#include <unordered_set>

namespace serdetk::xpath {
namespace {

void validate_tree(const CodeTree& tree, Diagnostics& diagnostics) {
    static const std::unordered_set<std::string> functions{
        "boolean", "count", "empty", "exists", "false", "last", "name", "not",
        "position", "string", "true"
    };
    if (tree.kind == NodeKind::Function && !functions.contains(tree.text))
        diagnostics.add({Diagnostic::Severity::Error, "err:XPST0017: unsupported function " + tree.text, {}});
    if (tree.kind == NodeKind::Path && tree.text == "relative" && tree.steps.empty())
        diagnostics.add({Diagnostic::Severity::Error, "err:XPST0003: empty relative path", {}});
    for (const auto& child : tree.children) validate_tree(*child, diagnostics);
    for (const auto& [key, value] : tree.entries) {
        validate_tree(*key, diagnostics);
        validate_tree(*value, diagnostics);
    }
    for (const auto& step : tree.steps)
        for (const auto& predicate : step.predicates) validate_tree(*predicate, diagnostics);
}

} // namespace

Diagnostics Validator::validate(const CodeTree& tree) const {
    Diagnostics diagnostics;
    validate_tree(tree, diagnostics);
    return diagnostics;
}

} // namespace serdetk::xpath
