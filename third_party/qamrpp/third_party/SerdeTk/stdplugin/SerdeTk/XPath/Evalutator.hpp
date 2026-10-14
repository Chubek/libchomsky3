#pragma once

#include "Adapter.hpp"
#include "CodeDAG.hpp"

namespace serdetk::xpath {

class Evalutator {
public:
    explicit Evalutator(const Adapter& adapter) : adapter_(adapter) {}
    [[nodiscard]] Sequence evaluate(const CodeTree& tree, DynamicContext context) const;
    [[nodiscard]] Sequence evaluate(const CodeDAG& dag, const Document& document) const;

private:
    [[nodiscard]] Sequence path(const CodeTree& tree, DynamicContext context) const;
    [[nodiscard]] Sequence binary(const CodeTree& tree, DynamicContext context) const;
    [[nodiscard]] Sequence function(const CodeTree& tree, DynamicContext context) const;
    [[nodiscard]] Sequence predicate(const Sequence& input, const CodeTree& expression, DynamicContext context) const;

    const Adapter& adapter_;
};

} // namespace serdetk::xpath
