#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace serdetk::xpath {

enum class NodeKind { Literal, ContextItem, Root, Path, Unary, Binary, Function, ArrayConstructor, MapConstructor };
enum class StepKind { Child, Descendant, Wildcard, Attribute, Lookup };

struct CodeTree;
using CodeTreePtr = std::shared_ptr<CodeTree>;

struct PathStep {
    StepKind kind {StepKind::Child};
    std::string name {};
    std::vector<CodeTreePtr> predicates {};
};

struct CodeTree {
    NodeKind kind {NodeKind::Literal};
    std::string text {};
    std::vector<CodeTreePtr> children {};
    std::vector<PathStep> steps {};
    std::vector<std::pair<CodeTreePtr, CodeTreePtr>> entries {};

    [[nodiscard]] static CodeTreePtr literal(std::string value);
    [[nodiscard]] static CodeTreePtr node(NodeKind kind, std::string text = {});
};

} // namespace serdetk::xpath
