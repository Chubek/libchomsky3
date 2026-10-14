#pragma once

#include "CodeTree.hpp"
#include "SerdeTk.hpp"

namespace serdetk::xpath {

class Validator {
public:
    [[nodiscard]] Diagnostics validate(const CodeTree& tree) const;
};

} // namespace serdetk::xpath
