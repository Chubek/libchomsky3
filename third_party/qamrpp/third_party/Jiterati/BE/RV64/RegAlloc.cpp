#include "RegAlloc.hpp"

namespace jiterati::be::rv64 {

// Greedy local allocation: hand out allocatable GPRs to virtual temporaries in
// first-seen order; once the register set is exhausted, spill subsequent
// temporaries to the stack frame.  This mirrors the simple allocator used by
// the other register backends and is sufficient for the linear TAC emitted by
// the current lowering.
void allocate_registers(MachineFunction& function) {
    auto regs = allocatable_gprs();
    std::size_t next = 0;
    for (auto& block : function.blocks) {
        for (auto& inst : block.instructions) {
            for (auto const& op : inst.operands) {
                if (op.rfind("%t", 0) != 0) continue;
                if (function.vreg_assignment.count(op) != 0) continue;
                if (next < regs.size()) {
                    function.vreg_assignment[op] = regs[next++];
                } else {
                    function.stack_size += 8;
                }
            }
        }
    }
}

} // namespace jiterati::be::rv64
