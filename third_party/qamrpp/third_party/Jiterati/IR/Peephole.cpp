#include "IR.hpp"

#include <cstdint>
#include <optional>

namespace jiterati::ir {
namespace {

std::optional<std::int64_t> as_i64(TACValue const& value) {
    if (value.kind != TACValue::Kind::Constant) return std::nullopt;
    if (auto const* integer = std::get_if<std::int64_t>(&value.constant)) return *integer;
    if (auto const* boolean = std::get_if<bool>(&value.constant)) return *boolean ? 1 : 0;
    return std::nullopt;
}

std::optional<std::int64_t> fold_i64(TACOpcode opcode, std::int64_t lhs, std::int64_t rhs) {
    switch (opcode) {
        case TACOpcode::Add: return lhs + rhs;
        case TACOpcode::Sub: return lhs - rhs;
        case TACOpcode::Mul: return lhs * rhs;
        case TACOpcode::SDiv: if (rhs != 0) return lhs / rhs; break;
        case TACOpcode::SRem: if (rhs != 0) return lhs % rhs; break;
        case TACOpcode::And: return lhs & rhs;
        case TACOpcode::Or: return lhs | rhs;
        case TACOpcode::Xor: return lhs ^ rhs;
        case TACOpcode::Shl: return lhs << rhs;
        case TACOpcode::AShr: return lhs >> rhs;
        case TACOpcode::Eq: return lhs == rhs ? 1 : 0;
        case TACOpcode::Ne: return lhs != rhs ? 1 : 0;
        case TACOpcode::SLt: return lhs < rhs ? 1 : 0;
        case TACOpcode::SLe: return lhs <= rhs ? 1 : 0;
        case TACOpcode::SGt: return lhs > rhs ? 1 : 0;
        case TACOpcode::SGe: return lhs >= rhs ? 1 : 0;
        default: break;
    }
    return std::nullopt;
}

bool is_binary(TACOpcode opcode) {
    switch (opcode) {
        case TACOpcode::Add:
        case TACOpcode::Sub:
        case TACOpcode::Mul:
        case TACOpcode::SDiv:
        case TACOpcode::SRem:
        case TACOpcode::And:
        case TACOpcode::Or:
        case TACOpcode::Xor:
        case TACOpcode::Shl:
        case TACOpcode::AShr:
        case TACOpcode::Eq:
        case TACOpcode::Ne:
        case TACOpcode::SLt:
        case TACOpcode::SLe:
        case TACOpcode::SGt:
        case TACOpcode::SGe:
            return true;
        default:
            return false;
    }
}

} // namespace

bool peephole(TACFunction& function) {
    bool changed = false;
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            if (instruction.result.has_value() && is_binary(instruction.opcode) && instruction.operands.size() == 2) {
                auto lhs = as_i64(instruction.operands[0]);
                auto rhs = as_i64(instruction.operands[1]);
                if (lhs.has_value() && rhs.has_value()) {
                    auto folded = fold_i64(instruction.opcode, *lhs, *rhs);
                    if (folded.has_value()) {
                        TACValue replacement = TACValue::constant_value(instruction.type, *folded);
                        rewrite_values(function, *instruction.result, replacement);
                        instruction.opcode = TACOpcode::Move;
                        instruction.operands = { replacement };
                        changed = true;
                    }
                }
            }
            if (instruction.opcode == TACOpcode::Select && instruction.operands.size() == 3) {
                if (auto cond = as_i64(instruction.operands[0]); cond.has_value()) {
                    TACValue replacement = *cond ? instruction.operands[1] : instruction.operands[2];
                    if (instruction.result.has_value()) rewrite_values(function, *instruction.result, replacement);
                    instruction.opcode = TACOpcode::Move;
                    instruction.operands = { replacement };
                    changed = true;
                }
            }
        }
    }
    return changed;
}

bool peephole(TACModule& module) {
    bool changed = false;
    for (auto& function : module.functions) changed = peephole(function) || changed;
    return changed;
}

} // namespace jiterati::ir
