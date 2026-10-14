#include "Emit.hpp"

#include <sstream>

namespace jiterati::be::aarch64 {
namespace {

std::string operand(MachineFunction const& function, std::string const& op) {
    auto it = function.vreg_assignment.find(op);
    if (it != function.vreg_assignment.end()) return reg_name(it->second);
    if (!op.empty() && op[0] == '%') return op.substr(1);
    return op;
}

} // namespace

std::string emit_assembly(MachineFunction const& function) {
    std::ostringstream out;
    out << "// AArch64 AAPCS64 assembly generated from Jiterati TAC\n";
    out << ".global " << function.name << "\n" << function.name << ":\n";
    out << "  stp x29, x30, [sp, #-16]!\n  mov x29, sp\n";
    if (function.stack_size != 0) out << "  sub sp, sp, #" << function.stack_size << "\n";

    for (auto const& block : function.blocks) {
        out << ".L" << block.label << ":\n";
        for (auto const& inst : block.instructions) {
            if (!inst.comment.empty()) out << "  // " << inst.comment << "\n";
            if (inst.opcode == "ret") {
                if (!inst.operands.empty()) out << "  mov x0, " << operand(function, inst.operands[0]) << "\n";
                if (function.stack_size != 0) out << "  add sp, sp, #" << function.stack_size << "\n";
                out << "  ldp x29, x30, [sp], #16\n  ret\n";
                continue;
            }
            if (inst.opcode == "nop") {
                out << "  nop\n";
                continue;
            }
            out << "  " << inst.opcode;
            for (std::size_t i = 0; i < inst.operands.size(); ++i) {
                out << (i == 0 ? " " : ", ") << operand(function, inst.operands[i]);
            }
            out << "\n";
        }
    }
    return out.str();
}

} // namespace jiterati::be::aarch64
