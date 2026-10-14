#pragma once

#include "CodeTree.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace serdetk::xpath {

class CodeDAG {
public:
    explicit CodeDAG(CodeTreePtr root = {}) : root_(std::move(root)) {}
    [[nodiscard]] const CodeTreePtr& root() const noexcept { return root_; }
    [[nodiscard]] static CodeDAG compile(CodeTreePtr tree);

private:
    CodeTreePtr root_;
};

} // namespace serdetk::xpath
