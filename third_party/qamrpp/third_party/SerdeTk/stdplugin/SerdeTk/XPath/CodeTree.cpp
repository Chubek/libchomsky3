#include "CodeTree.hpp"

namespace serdetk::xpath {

CodeTreePtr CodeTree::literal(std::string value) {
    return node(NodeKind::Literal, std::move(value));
}

CodeTreePtr CodeTree::node(NodeKind kind, std::string text) {
    auto result = std::make_shared<CodeTree>();
    result->kind = kind;
    result->text = std::move(text);
    return result;
}

} // namespace serdetk::xpath
