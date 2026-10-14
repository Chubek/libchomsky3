#include "Emit.hpp"

#include <sstream>

namespace jiterati::be::rv64 {
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
    out << "# RV64 LP64 assembly generated from Jiterati TAC\n";
    out << ".globl " << function.name << "\n" << function.name << ":\n";
    if (function.stack_size != 0) out << "  addi sp, sp, -" << function.stack_size << "\n";

    for (auto const& block : function.blocks) {
        out << ".L" << block.label << ":\n";
        for (auto const& inst : block.instructions) {
            if (!inst.comment.empty()) out << "  # " << inst.comment << "\n";
            if (inst.opcode == "ret") {
                if (!inst.operands.empty()) out << "  mv a0, " << operand(function, inst.operands[0]) << "\n";
                if (function.stack_size != 0) out << "  addi sp, sp, " << function.stack_size << "\n";
                out << "  ret\n";
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

} // namespace jiterati::be::rv64
