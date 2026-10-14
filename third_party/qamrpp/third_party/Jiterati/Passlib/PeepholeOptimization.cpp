/** @file Passlib/PeepholeOptimization.cpp
 *  @brief Simple peephole optimizations.
 */
#include "Jiterati.hpp"
#include "Jiterati-Pass.hpp"

namespace jiterati {

class PeepholeOptimization : public TransformPass, public FunctionPass {
public:
    std::string name() const override { return "peephole-optimization"; }

    void run(Function& fn) override {
        // Single pass: x + 0, x - 0, x & 0xFF..., x ^ 0, x | 0.
        for (auto& inst : fn.instr_storage()) {
            if (inst.operands().size() < 2) continue;
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

            switch (inst.opcode()) {
                case Opcode::Add:
                case Opcode::Sub:
                case Opcode::Or:
                    if (v == 0) inst = Instruction(inst.id(), Opcode::Copy, inst.type(), { a });
                    break;
                case Opcode::Xor:
                    if (v == 0) inst = Instruction(inst.id(), Opcode::Copy, inst.type(), { a });
                    break;
                case Opcode::Mul:
                    if (v == 0) {
                        Value zero = fn.const_i32(0);
                        inst = Instruction(inst.id(), Opcode::Copy, inst.type(), { zero });
                    }
                    break;
                default:
                    break;
            }
        }
    }
};

} // namespace jiterati
