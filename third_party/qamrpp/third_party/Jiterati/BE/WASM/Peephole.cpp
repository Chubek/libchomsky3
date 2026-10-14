#include "Peephole.hpp"

namespace jiterati::be::wasm {

bool peephole(WasmFunction& function) {
    bool changed = false;
    for (auto& block : function.blocks) {
        for (auto& inst : block.instructions) {
            // local.set $x $x is a no-op store.
            if (inst.opcode == "local.set" && inst.operands.size() >= 2 && inst.operands[0] == inst.operands[1]) {
                inst.opcode = "nop";
                inst.operands.clear();
                changed = true;
                continue;
            }
            // i64.or / i64.and of a value with itself is an identity -> local copy.
            if ((inst.opcode == "i64.or" || inst.opcode == "i64.and") &&
                inst.operands.size() >= 3 && inst.operands[1] == inst.operands[2]) {
                inst.opcode = "local.set";
                inst.operands = { inst.operands[0], inst.operands[1] };
                changed = true;
            }
        }
    }
    return changed;
}

} // namespace jiterati::be::wasm
