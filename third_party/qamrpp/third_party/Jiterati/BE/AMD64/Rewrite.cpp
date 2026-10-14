#include "Rewrite.hpp"

namespace jiterati::be::amd64 {

void rewrite(MachineFunction& function) {
    for (auto& block : function.blocks) {
        for (auto& inst : block.instructions) {
            if (inst.opcode == "idiv" || inst.opcode == "div" || inst.opcode == "idiv.rem" || inst.opcode == "div.rem") {
                inst.comment += " | amd64 uses rdx:rax fixed dividend/result registers";
            }
            if ((inst.opcode == "shl" || inst.opcode == "shr" || inst.opcode == "sar") && inst.operands.size() > 2) {
                inst.comment += " | variable shift count must be in cl";
            }
        }
    }
}

} // namespace jiterati::be::amd64
