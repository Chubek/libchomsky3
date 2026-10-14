#include "Peephole.hpp"

namespace jiterati::be::aarch64 {

bool peephole(MachineFunction& function) {
    bool changed = false;
    for (auto& block : function.blocks) {
        for (auto& inst : block.instructions) {
            // Self-move collapses to nop.
            if (inst.opcode == "mov" && inst.operands.size() >= 2 && inst.operands[0] == inst.operands[1]) {
                inst.opcode = "nop";
                inst.operands.clear();
                changed = true;
                continue;
            }
            // orr rd, rs, rs and eor rd, rs, rs are identities -> move.
            if (inst.opcode == "orr" && inst.operands.size() >= 3 && inst.operands[1] == inst.operands[2]) {
                inst.opcode = "mov";
                inst.operands = { inst.operands[0], inst.operands[1] };
                changed = true;
                continue;
            }
            // add rd, rs, #0 -> mov rd, rs.
            if (inst.opcode == "add" && inst.operands.size() >= 3 && inst.operands[2] == "0") {
                inst.opcode = "mov";
                inst.operands = { inst.operands[0], inst.operands[1] };
                changed = true;
            }
        }
    }
    return changed;
}

} // namespace jiterati::be::aarch64
