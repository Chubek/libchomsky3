/** @file Passlib/ConstantFolding.cpp
 *  @brief Constant folding transform pass.
 */
#include "Jiterati.hpp"
#include "Jiterati-Pass.hpp"

#include <functional>

namespace jiterati {

namespace {

std::optional<int64_t> as_int(Function& fn, Value v) {
    if (!v.is_const()) return std::nullopt;
    auto c = fn.const_value(v.const_index());
    int64_t result = 0;
    bool ok = false;
    std::visit([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_integral_v<T>) {
            result = static_cast<int64_t>(val);
            ok = true;
        }
    }, c);
    if (!ok) return std::nullopt;
    return result;
}

} // namespace

class ConstantFolding : public TransformPass, public FunctionPass {
public:
    std::string name() const override { return "constant-folding"; }

    void run(Function& fn) override {
        for (auto& inst : fn.instr_storage()) {
            if (inst.type().is_void()) continue;
            auto a = inst.operands().size() > 0 ? as_int(fn, inst.operand(0)) : std::nullopt;
            auto b = inst.operands().size() > 1 ? as_int(fn, inst.operand(1)) : std::nullopt;
            std::optional<int64_t> result;

            switch (inst.opcode()) {
                case Opcode::Add:
                    if (a && b) result = *a + *b;
                    break;
                case Opcode::Sub:
                    if (a && b) result = *a - *b;
                    break;
                case Opcode::Mul:
                    if (a && b) result = *a * *b;
                    break;
                case Opcode::SDiv:
                    if (a && b && *b != 0) result = *a / *b;
                    break;
                case Opcode::SRem:
                    if (a && b && *b != 0) result = *a % *b;
                    break;
                case Opcode::And:
                    if (a && b) result = *a & *b;
                    break;
                case Opcode::Or:
                    if (a && b) result = *a | *b;
                    break;
                case Opcode::Xor:
                    if (a && b) result = *a ^ *b;
                    break;
                case Opcode::Neg:
                    if (a) result = -*a;
                    break;
                case Opcode::Not:
                    if (a) result = ~*a;
                    break;
                case Opcode::Shl:
                    if (a && b) result = *a << static_cast<int>(*b);
                    break;
                case Opcode::LShr:
                    if (a && b) result = static_cast<uint64_t>(*a) >> static_cast<int>(*b);
                    break;
                case Opcode::AShr:
                    if (a && b) result = *a >> static_cast<int>(*b);
                    break;
                case Opcode::ICmpEq:
                    if (a && b) result = (*a == *b) ? 1 : 0;
                    break;
                case Opcode::ICmpNe:
                    if (a && b) result = (*a != *b) ? 1 : 0;
                    break;
                case Opcode::ICmpSLt:
                    if (a && b) result = (*a < *b) ? 1 : 0;
                    break;
                case Opcode::ICmpSLe:
                    if (a && b) result = (*a <= *b) ? 1 : 0;
                    break;
                case Opcode::ICmpSGt:
                    if (a && b) result = (*a > *b) ? 1 : 0;
                    break;
                case Opcode::ICmpSGe:
                    if (a && b) result = (*a >= *b) ? 1 : 0;
                    break;
                default:
                    break;
            }

            if (result) {
                Value repl = inst.type().size() <= 4
                    ? fn.const_i32(static_cast<int32_t>(*result))
                    : fn.const_i64(*result);
                // Replace all uses: rewrite operands in later instructions.
                Value old_v = Value::vreg(inst.id());
                for (auto& later : fn.instr_storage()) {
                    for (auto& op : later.operands()) {
                        if (op == old_v) op = repl;
                    }
                }
                // Mark instruction as dead by turning it into a copy of the constant.
                inst = Instruction(inst.id(), Opcode::Copy, inst.type(), { repl });
            }
        }
    }
};

} // namespace jiterati
