#include "Rewrite.hpp"

#include <string>
#include <vector>

namespace jiterati::be::aarch64 {
namespace {

using Insts = std::vector<MachineInst>;

void append(Insts& out, std::string op, std::vector<std::string> operands) {
    MachineInst mi;
    mi.opcode = std::move(op);
    mi.operands = std::move(operands);
    out.push_back(std::move(mi));
}

// Lower a cset.<cond> rd, a, b pseudo into the real cmp+cset pair.  Returns
// true when the instruction was rewritten.
bool expand_comparison(MachineInst const& inst, Insts& out) {
    if (inst.opcode.rfind("cset.", 0) != 0) return false;
    if (inst.operands.size() < 3) return false;
    std::string cond = inst.opcode.substr(5); // text after "cset."
    std::string const& rd = inst.operands[0];
    std::string const& a = inst.operands[1];
    std::string const& b = inst.operands[2];
    append(out, "cmp", { a, b });
    append(out, "cset", { rd, cond });
    if (!inst.comment.empty()) out.front().comment = inst.comment;
    return true;
}

// Lower srem/urem (no direct AArch64 form) into (s|u)div + msub:
//   rem rd, a, b ->  (s|u)div rd, a, b ; msub rd, rd, b, a
bool expand_remainder(MachineInst const& inst, Insts& out) {
    std::string divop;
    if (inst.opcode == "srem") divop = "sdiv";
    else if (inst.opcode == "urem") divop = "udiv";
    else return false;
    if (inst.operands.size() < 3) return false;
    std::string const& rd = inst.operands[0];
    std::string const& a = inst.operands[1];
    std::string const& b = inst.operands[2];
    append(out, divop, { rd, a, b });
    append(out, "msub", { rd, rd, b, a });
    if (!inst.comment.empty()) out.front().comment = inst.comment;
    return true;
}

} // namespace

void rewrite(MachineFunction& function) {
    for (auto& block : function.blocks) {
        Insts lowered;
        lowered.reserve(block.instructions.size());
        for (auto& inst : block.instructions) {
            Insts expansion;
            if (expand_comparison(inst, expansion) || expand_remainder(inst, expansion)) {
                for (auto& mi : expansion) lowered.push_back(std::move(mi));
            } else {
                if (inst.opcode == "csel") {
                    inst.comment += " | select needs flag conversion (tst/cset) before csel";
                }
                lowered.push_back(std::move(inst));
            }
        }
        block.instructions = std::move(lowered);
    }
}

} // namespace jiterati::be::aarch64
