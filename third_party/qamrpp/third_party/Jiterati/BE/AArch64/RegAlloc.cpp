#include "RegAlloc.hpp"

namespace jiterati::be::aarch64 {

// Greedy local allocation: assign allocatable GPRs to virtual temporaries in
// first-seen order and spill the overflow to the stack frame.  Matches the
// simple allocator shared across the register backends.
void allocate_registers(MachineFunction& function) {
    auto regs = allocatable_gprs();
    std::size_t next = 0;
    for (auto& block : function.blocks) {
        for (auto& inst : block.instructions) {
            for (auto const& op : inst.operands) {
                if (op.rfind("%t", 0) != 0 || function.vreg_assignment.count(op) != 0) continue;
                if (next < regs.size()) {
                    function.vreg_assignment[op] = regs[next++];
                } else {
                    function.stack_size += 8;
                }
            }
        }
    }
}

} // namespace jiterati::be::aarch64
