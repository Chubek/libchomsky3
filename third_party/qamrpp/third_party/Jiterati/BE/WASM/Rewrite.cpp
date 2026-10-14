#include "Rewrite.hpp"

namespace jiterati::be::wasm {

// Annotate the constructs that need further lowering before this text could be
// assembled into a valid WASM module.  WASM lacks direct neg/not (lower to
// 0 - x and x ^ -1 respectively) and uses structured control flow rather than
// free branches, so the relevant opcodes are flagged.
void rewrite(WasmFunction& function) {
    for (auto& block : function.blocks) {
        for (auto& inst : block.instructions) {
            if (inst.opcode == "i64.neg") {
                inst.comment += " | lower as (i64.const 0; <x>; i64.sub)";
            } else if (inst.opcode == "i64.not") {
                inst.comment += " | lower as (<x>; i64.const -1; i64.xor)";
            } else if (inst.opcode == "phi") {
                inst.comment += " | WASM has no phi; resolve via block params / locals";
            } else if (inst.opcode == "br" || inst.opcode == "br_if") {
                inst.comment += " | must target a structured block label, not a free target";
            } else if (inst.opcode.rfind("jiterati.", 0) == 0) {
                inst.comment += " | requires structured-control or helper lowering for wasm";
            }
        }
    }
}

} // namespace jiterati::be::wasm
