#include "ISel.hpp"

namespace jiterati::be::rv64 {
namespace {

// Map a TAC opcode to an RV64 mnemonic.  Comparisons that RV64 lacks as single
// instructions (sle/sgt/sge and the unsigned variants, plus eq/ne between two
// values) are emitted as pseudo-ops here and lowered to real instruction
// sequences by rewrite().  Neg/not/mv/li/j/bnez/ret are standard assembler
// pseudos.
std::string opcode(ir::TACOpcode op) {
    switch (op) {
        // Integer arithmetic.
        case ir::TACOpcode::Add:  return "add";
        case ir::TACOpcode::Sub:  return "sub";
        case ir::TACOpcode::Mul:  return "mul";
        case ir::TACOpcode::SDiv: return "div";
        case ir::TACOpcode::UDiv: return "divu";
        case ir::TACOpcode::SRem: return "rem";
        case ir::TACOpcode::URem: return "remu";
        // Bitwise / shifts.
        case ir::TACOpcode::And:  return "and";
        case ir::TACOpcode::Or:   return "or";
        case ir::TACOpcode::Xor:  return "xor";
        case ir::TACOpcode::Shl:  return "sll";
        case ir::TACOpcode::LShr: return "srl";
        case ir::TACOpcode::AShr: return "sra";
        // Unary (assembler pseudos: neg = sub x0; not = xori -1).
        case ir::TACOpcode::Neg:  return "neg";
        case ir::TACOpcode::Not:  return "not";
        // Comparisons.  slt/sltu are native; the rest are pseudos for rewrite().
        case ir::TACOpcode::Eq:   return "seq";
        case ir::TACOpcode::Ne:   return "sne";
        case ir::TACOpcode::SLt:  return "slt";
        case ir::TACOpcode::SLe:  return "sle";
        case ir::TACOpcode::SGt:  return "sgt";
        case ir::TACOpcode::SGe:  return "sge";
        case ir::TACOpcode::ULt:  return "sltu";
        case ir::TACOpcode::ULe:  return "ule";
        case ir::TACOpcode::UGt:  return "ugt";
        case ir::TACOpcode::UGe:  return "uge";
        // Data movement / control flow.
        case ir::TACOpcode::Const:      return "li";
        case ir::TACOpcode::Move:       return "mv";
        case ir::TACOpcode::Select:     return "select";
        case ir::TACOpcode::Phi:        return "phi";
        case ir::TACOpcode::Nop:        return "nop";
        case ir::TACOpcode::Call:       return "jalr";
        case ir::TACOpcode::Branch:     return "j";
        case ir::TACOpcode::CondBranch: return "bnez";
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

} // namespace jiterati::be::rv64
