#include "CodeDAG.hpp"

#include <unordered_map>

namespace serdetk::xpath {
namespace {

std::string signature(const CodeTree& tree) {
    std::string out = std::to_string(static_cast<int>(tree.kind)) + ":" + tree.text;
    for (const auto& step : tree.steps) {
        out += "/" + std::to_string(static_cast<int>(step.kind)) + ":" + step.name;
        for (const auto& predicate : step.predicates) out += "[" + signature(*predicate) + "]";
    }
    for (const auto& child : tree.children) out += "(" + signature(*child) + ")";
    for (const auto& [key, value] : tree.entries) out += "{" + signature(*key) + ":" + signature(*value) + "}";
    return out;
}

CodeTreePtr intern(const CodeTreePtr& tree, std::unordered_map<std::string, CodeTreePtr>& nodes) {
    auto copy = std::make_shared<CodeTree>(*tree);
    for (auto& child : copy->children) child = intern(child, nodes);
    for (auto& [key, value] : copy->entries) {
        key = intern(key, nodes);
        value = intern(value, nodes);
    }
    for (auto& step : copy->steps)
        for (auto& predicate : step.predicates) predicate = intern(predicate, nodes);
    const auto key = signature(*copy);
    if (const auto found = nodes.find(key); found != nodes.end()) return found->second;
    nodes.emplace(key, copy);
    return copy;
}

} // namespace

CodeDAG CodeDAG::compile(CodeTreePtr tree) {
    std::unordered_map<std::string, CodeTreePtr> nodes;
    return CodeDAG(intern(tree, nodes));
}

} // namespace serdetk::xpath
