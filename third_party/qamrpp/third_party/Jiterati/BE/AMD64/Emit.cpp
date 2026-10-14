#include "Emit.hpp"

#include <sstream>

namespace jiterati::be::amd64 {
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
    out << "; AMD64 SysV assembly generated from Jiterati TAC\n";
    out << "global " << function.name << "\n" << function.name << ":\n";
    out << "  push rbp\n  mov rbp, rsp\n";
    if (function.stack_size != 0) out << "  sub rsp, " << function.stack_size << "\n";
    for (auto const& block : function.blocks) {
        out << "." << block.label << ":\n";
        for (auto const& inst : block.instructions) {
            if (!inst.comment.empty()) out << "  ; " << inst.comment << "\n";
            if (inst.opcode == "ret") {
                if (!inst.operands.empty()) out << "  mov rax, " << operand(function, inst.operands[0]) << "\n";
                if (function.stack_size != 0) out << "  add rsp, " << function.stack_size << "\n";
                out << "  pop rbp\n  ret\n";
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

} // namespace jiterati::be::amd64
