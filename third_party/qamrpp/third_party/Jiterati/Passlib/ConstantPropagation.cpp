/** @file Passlib/ConstantPropagation.cpp
 *  @brief Simple forward constant propagation.
 */
#include "../include/Jiterati.hpp"
#include "../include/Jiterati-Pass.hpp"

#include <unordered_map>

namespace jiterati {

class ConstantPropagation : public TransformPass, public FunctionPass {
public:
    std::string name() const override { return "constant-propagation"; }

    void run(Function& fn) override {
        std::unordered_map<uint32_t, Value> constants;

        for (auto& inst : fn.instr_storage()) {
            if (inst.opcode() == Opcode::Copy && inst.operands().size() == 1 &&
                inst.operand(0).is_const()) {
                constants[inst.id()] = inst.operand(0);
            }

            for (auto& op : inst.operands()) {
                if (op.is_vreg()) {
                    auto it = constants.find(op.vreg_index());
                    if (it != constants.end()) op = it->second;
                }
            }

            if ((inst.opcode() == Opcode::Add || inst.opcode() == Opcode::Sub ||
                 inst.opcode() == Opcode::Mul || inst.opcode() == Opcode::And ||
                 inst.opcode() == Opcode::Or  || inst.opcode() == Opcode::Xor) &&
                inst.operands().size() == 2 && inst.operand(0).is_const() && inst.operand(1).is_const()) {
                constants[inst.id()] = inst.operand(0);
            }
        }
    }
};

} // namespace jiterati
