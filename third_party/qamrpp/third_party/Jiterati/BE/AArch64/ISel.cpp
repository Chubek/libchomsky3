#include "ISel.hpp"

namespace jiterati::be::aarch64 {
namespace {

// Map a TAC opcode to an AArch64 mnemonic.  Comparisons are emitted as
// cset.<cond> pseudos and lowered to cmp+cset by rewrite().  srem/urem have no
// direct AArch64 form and are lowered to (s|u)div+msub.  csel (select) needs
// flags rather than a 0/1 value and is left as a noted pseudo.
std::string opcode(ir::TACOpcode op) {
    switch (op) {
        // Integer arithmetic.
        case ir::TACOpcode::Add:  return "add";
        case ir::TACOpcode::Sub:  return "sub";
        case ir::TACOpcode::Mul:  return "mul";
        case ir::TACOpcode::SDiv: return "sdiv";
        case ir::TACOpcode::UDiv: return "udiv";
        case ir::TACOpcode::SRem: return "srem";
        case ir::TACOpcode::URem: return "urem";
        // Bitwise / shifts.
        case ir::TACOpcode::And:  return "and";
        case ir::TACOpcode::Or:   return "orr";
        case ir::TACOpcode::Xor:  return "eor";
        case ir::TACOpcode::Shl:  return "lsl";
        case ir::TACOpcode::LShr: return "lsr";
        case ir::TACOpcode::AShr: return "asr";
        // Unary.
        case ir::TACOpcode::Neg:  return "neg";
        case ir::TACOpcode::Not:  return "mvn";
        // Comparisons (condition appended after the dot).
        case ir::TACOpcode::Eq:   return "cset.eq";
        case ir::TACOpcode::Ne:   return "cset.ne";
        case ir::TACOpcode::SLt:  return "cset.lt";
        case ir::TACOpcode::SLe:  return "cset.le";
        case ir::TACOpcode::SGt:  return "cset.gt";
        case ir::TACOpcode::SGe:  return "cset.ge";
        case ir::TACOpcode::ULt:  return "cset.lo";
        case ir::TACOpcode::ULe:  return "cset.ls";
        case ir::TACOpcode::UGt:  return "cset.hi";
        case ir::TACOpcode::UGe:  return "cset.hs";
        // Data movement / control flow.
        case ir::TACOpcode::Const:      return "mov";
        case ir::TACOpcode::Move:       return "mov";
        case ir::TACOpcode::Select:     return "csel";
        case ir::TACOpcode::Phi:        return "phi";
        case ir::TACOpcode::Nop:        return "nop";
        case ir::TACOpcode::Call:       return "blr";
        case ir::TACOpcode::Branch:     return "b";
        case ir::TACOpcode::CondBranch: return "cbnz";
        default: return ir::tac_opcode_str(op);
    }
}

} // namespace

MachineFunction select(ir::TACFunction const& function) {
    MachineFunction out;
    out.name = function.name;
    for (auto const& block : function.blocks) {
        MachineBlock machine_block;
        machine_block.label = block.label();
        for (auto const& instruction : block.instructions) {
            MachineInst inst;
            inst.comment = instruction.str();
            if (instruction.opcode == ir::TACOpcode::Return) {
                inst.opcode = "ret";
                if (!instruction.operands.empty()) inst.operands = { instruction.operands[0].str() };
            } else {
                inst.opcode = opcode(instruction.opcode);
                if (instruction.result.has_value()) inst.operands.push_back(instruction.result->str());
                for (auto const& operand : instruction.operands) inst.operands.push_back(operand.str());
                for (auto target : instruction.targets) inst.operands.push_back("bb" + std::to_string(target));
                if (!instruction.symbol.empty()) inst.operands.push_back(instruction.symbol);
            }
            machine_block.instructions.push_back(std::move(inst));
        }
        out.blocks.push_back(std::move(machine_block));
    }
    return out;
}

} // namespace jiterati::be::aarch64
