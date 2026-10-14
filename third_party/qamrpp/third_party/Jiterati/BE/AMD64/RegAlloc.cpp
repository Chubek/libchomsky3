#include "RegAlloc.hpp"

namespace jiterati::be::amd64 {

void allocate_registers(MachineFunction& function) {
    auto regs = allocatable_gprs();
    std::size_t next = 0;
    for (auto& block : function.blocks) {
        for (auto& inst : block.instructions) {
            for (auto const& operand : inst.operands) {
                if (operand.rfind("%t", 0) != 0) continue;
                if (function.vreg_assignment.count(operand) != 0) continue;
                if (next < regs.size()) {
                    function.vreg_assignment[operand] = regs[next++];
                } else {
                    function.stack_size += 8;
                }
            }
        }
    }
}

} // namespace jiterati::be::amd64
