#pragma once

#include "CodeTree.hpp"

#include <string>

namespace serdetk::xpath {

class PrettyPrint {
public:
    [[nodiscard]] std::string format(const CodeTree& tree) const;
};

} // namespace serdetk::xpath
