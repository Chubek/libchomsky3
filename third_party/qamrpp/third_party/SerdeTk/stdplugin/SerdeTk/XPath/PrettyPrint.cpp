#include "PrettyPrint.hpp"

namespace serdetk::xpath {
namespace {

std::string format_tree(const CodeTree& tree) {
    switch (tree.kind) {
    case NodeKind::Literal: return "'" + tree.text + "'";
    case NodeKind::ContextItem: return ".";
    case NodeKind::Root: return "/";
    case NodeKind::Unary: return tree.text + format_tree(*tree.children.front());
    case NodeKind::Binary: return format_tree(*tree.children[0]) + " " + tree.text + " " + format_tree(*tree.children[1]);
    case NodeKind::Function: {
        std::string out = tree.text + "(";
        for (std::size_t index = 0; index < tree.children.size(); ++index) {
            if (index) out += ", ";
            out += format_tree(*tree.children[index]);
        }
        return out + ")";
    }
    case NodeKind::ArrayConstructor: {
        std::string out = "[";
        for (std::size_t index = 0; index < tree.children.size(); ++index) {
            if (index) out += ", ";
            out += format_tree(*tree.children[index]);
        }
        return out + "]";
    }
    case NodeKind::MapConstructor: return "map { ... }";
    case NodeKind::Path: {
        std::string out = tree.text == "absolute" ? "/" : "";
        for (std::size_t index = 0; index < tree.steps.size(); ++index) {
            const auto& step = tree.steps[index];
            if (index && step.kind != StepKind::Descendant) out += "/";
            if (step.kind == StepKind::Descendant) out += "//";
            if (step.kind == StepKind::Attribute) out += "@";
            if (step.kind == StepKind::Lookup) out += "?";
            out += step.name;
            for (const auto& predicate : step.predicates) out += "[" + format_tree(*predicate) + "]";
        }
        return out;
    }
    }
    return {};
}

} // namespace

std::string PrettyPrint::format(const CodeTree& tree) const { return format_tree(tree); }

} // namespace serdetk::xpath
