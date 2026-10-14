/** @file Passlib/DeadCodeRemoval.cpp
 *  @brief Remove instructions with no side effects whose results are unused.
 */
#include "Jiterati.hpp"
#include "Jiterati-Pass.hpp"

#include <unordered_set>

namespace jiterati {

class DeadCodeRemoval : public TransformPass, public FunctionPass {
public:
    std::string name() const override { return "dead-code-removal"; }

    void run(Function& fn) override {
        std::unordered_set<uint32_t> used;
        for (auto const& inst : fn.instr_storage()) {
            for (auto const& op : inst.operands()) {
                if (op.is_vreg()) used.insert(op.vreg_index());
            }
        }

        for (auto& inst : fn.instr_storage()) {
            if (inst.type().is_void()) continue;
            if (used.count(inst.id())) continue;
            if (has_side_effects(inst.opcode())) continue;
            // Replace with copy-of-none to neutralize.
            inst = Instruction(inst.id(), Opcode::Copy, inst.type(), { Value::none() });
        }
    }

private:
    static bool has_side_effects(Opcode op) {
        switch (op) {
            case Opcode::Store:
            case Opcode::Call:
            case Opcode::Ret:
            case Opcode::Br:
            case Opcode::CondBr:
            case Opcode::Switch:
            case Opcode::Unreachable:
                return true;
            default:
                return false;
        }
    }
};

} // namespace jiterati
