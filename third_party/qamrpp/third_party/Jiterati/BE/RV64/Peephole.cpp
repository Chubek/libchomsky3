#include "Peephole.hpp"

namespace jiterati::be::rv64 {

bool peephole(MachineFunction& function) {
    bool changed = false;
    for (auto& block : function.blocks) {
        for (auto& inst : block.instructions) {
            // Self-moves and no-op arithmetic identities collapse to nop.
            if (inst.opcode == "mv" && inst.operands.size() >= 2 && inst.operands[0] == inst.operands[1]) {
                inst.opcode = "nop";
                inst.operands.clear();
                changed = true;
                continue;
            }
            if (inst.opcode == "addi" && inst.operands.size() >= 3 &&
                inst.operands[0] == inst.operands[1] && inst.operands[2] == "0") {
                inst.opcode = "nop";
                inst.operands.clear();
                changed = true;
                continue;
            }
            // x | x and x & x are identities -> plain move.
            if ((inst.opcode == "or" || inst.opcode == "and") && inst.operands.size() >= 3 &&
                inst.operands[1] == inst.operands[2]) {
                inst.opcode = "mv";
                inst.operands = { inst.operands[0], inst.operands[1] };
                changed = true;
                continue;
            }
            // add rd, rs, zero -> mv rd, rs.
            if (inst.opcode == "add" && inst.operands.size() >= 3 && inst.operands[2] == "0") {
                inst.opcode = "mv";
                inst.operands = { inst.operands[0], inst.operands[1] };
                changed = true;
            }
        }
    }
    return changed;
}

} // namespace jiterati::be::rv64
