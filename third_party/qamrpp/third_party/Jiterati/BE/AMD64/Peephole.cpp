#include "Peephole.hpp"

namespace jiterati::be::amd64 {

bool peephole(MachineFunction& function) {
    bool changed = false;
    for (auto& block : function.blocks) {
        for (auto& inst : block.instructions) {
            if (inst.opcode == "mov" && inst.operands.size() >= 2 && inst.operands[0] == inst.operands[1]) {
                inst.opcode = "nop";
                inst.operands.clear();
                changed = true;
            }
            if (inst.opcode == "add" && inst.operands.size() >= 3 && inst.operands[2] == "0") {
                inst.opcode = "mov";
                inst.operands.erase(inst.operands.begin() + 2);
                changed = true;
            }
        }
    }
    return changed;
}

} // namespace jiterati::be::amd64
