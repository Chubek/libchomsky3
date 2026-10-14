#include "Rewrite.hpp"

#include <string>
#include <vector>

namespace jiterati::be::rv64 {
namespace {

using Insts = std::vector<MachineInst>;

// Expand a comparison pseudo-op into the real RV64 instruction sequence.  RV64
// provides only slt/sltu (and the unary seqz/snez); ordered comparisons and
// two-operand equality are synthesized from them.  Returns true (and fills
// `out`) when `inst` is a recognized comparison pseudo that has been rewritten.
bool expand_comparison(MachineInst const& inst, Insts& out) {
    auto const& ops = inst.operands;
    if (ops.size() < 3) return false;
    std::string const& rd = ops[0];
    std::string const& a = ops[1];
    std::string const& b = ops[2];

    auto emit = [&](std::string op, std::vector<std::string> operands) {
        MachineInst mi;
        mi.opcode = std::move(op);
        mi.operands = std::move(operands);
        out.push_back(std::move(mi));
    };

    if (inst.opcode == "seq") {
        emit("xor", { rd, a, b });
        emit("seqz", { rd, rd });
    } else if (inst.opcode == "sne") {
        emit("xor", { rd, a, b });
        emit("snez", { rd, rd });
    } else if (inst.opcode == "slt" || inst.opcode == "sltu") {
        return false; // native single instruction
    } else if (inst.opcode == "sgt") {
        emit("slt", { rd, b, a });           // a > b  <=>  b < a
    } else if (inst.opcode == "ugt") {
        emit("sltu", { rd, b, a });
    } else if (inst.opcode == "sle") {
        emit("slt", { rd, b, a });           // a <= b <=> !(b < a)
        emit("xori", { rd, rd, "1" });
    } else if (inst.opcode == "ule") {
        emit("sltu", { rd, b, a });
        emit("xori", { rd, rd, "1" });
    } else if (inst.opcode == "sge") {
        emit("slt", { rd, a, b });           // a >= b <=> !(a < b)
        emit("xori", { rd, rd, "1" });
    } else if (inst.opcode == "uge") {
        emit("sltu", { rd, a, b });
        emit("xori", { rd, rd, "1" });
    } else {
        return false;
    }

    // Preserve the original TAC comment on the first synthesized instruction.
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
            if (expand_comparison(inst, expansion)) {
                for (auto& mi : expansion) lowered.push_back(std::move(mi));
            } else {
                if (inst.opcode == "select") {
                    inst.comment += " | lower select with a branch or arith/mask sequence";
                }
                lowered.push_back(std::move(inst));
            }
        }
        block.instructions = std::move(lowered);
    }
}

} // namespace jiterati::be::rv64
