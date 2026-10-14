#include "IR.hpp"

namespace jiterati::ir {
namespace {

bool same_value(TACValue const& lhs, TACValue const& rhs) {
    if (lhs.kind != rhs.kind || lhs.index != rhs.index || lhs.type != rhs.type) return false;
    if (lhs.kind == TACValue::Kind::Constant) return lhs.constant == rhs.constant;
    if (lhs.kind == TACValue::Kind::Parameter) return lhs.name == rhs.name;
    return true;
}

} // namespace

void rewrite_values(TACFunction& function, TACValue const& from, TACValue const& to) {
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            for (auto& operand : instruction.operands) {
                if (same_value(operand, from)) operand = to;
            }
        }
    }
}

} // namespace jiterati::ir
