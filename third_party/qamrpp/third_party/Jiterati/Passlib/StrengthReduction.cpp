/** @file Passlib/StrengthReduction.cpp
 *  @brief Strength reduction transform.
 */
#include "Jiterati.hpp"
#include "Jiterati-Pass.hpp"

namespace jiterati {

class StrengthReduction : public TransformPass, public FunctionPass {
public:
    std::string name() const override { return "strength-reduction"; }

    void run(Function& fn) override {
        for (auto& inst : fn.instr_storage()) {
            if (inst.opcode() != Opcode::Mul || inst.operands().size() < 2) continue;
            auto const& a = inst.operand(0);
            auto const& b = inst.operand(1);
            if (!b.is_const()) continue;
            auto c = fn.const_value(b.const_index());
            int64_t v = 0;
            bool ok = false;
            std::visit([&](auto&& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_integral_v<T>) { v = static_cast<int64_t>(val); ok = true; }
            }, c);
            if (!ok) continue;

            // x * 0 -> 0, x * 1 -> x, x * 2 -> x + x, x * (2^k) -> x << k
            if (v == 0) {
                Value zero = fn.const_i32(0);
                inst = Instruction(inst.id(), Opcode::Copy, inst.type(), { zero });
            } else if (v == 1) {
                inst = Instruction(inst.id(), Opcode::Copy, inst.type(), { a });
            } else if (v == 2) {
                inst = Instruction(inst.id(), Opcode::Add, inst.type(), { a, a });
            } else {
                int shift = 0;
                int64_t t = v;
                while (t > 1 && (t & 1) == 0) { t >>= 1; ++shift; }
                if (t == 1) {
                    Value shift_val = fn.const_i32(shift);
                    inst = Instruction(inst.id(), Opcode::Shl, inst.type(), { a, shift_val });
                }
            }
        }
    }
};

} // namespace jiterati
