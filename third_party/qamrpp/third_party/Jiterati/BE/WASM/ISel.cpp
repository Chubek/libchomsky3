#include "ISel.hpp"

namespace jiterati::be::wasm {
namespace {

// Map a TAC opcode to a WASM opcode name.  The backend operates on i64 values.
// neg/not have no direct WASM form and are emitted as pseudos lowered in
// rewrite() (neg = 0 - x, not = x ^ -1).  Everything else maps to a real WASM
// instruction name; the emitted text is an illustrative (non-stack-correct)
// translation rather than loadable WAT.
std::string opcode(ir::TACOpcode op) {
    switch (op) {
        // Integer arithmetic.
        case ir::TACOpcode::Add:  return "i64.add";
        case ir::TACOpcode::Sub:  return "i64.sub";
        case ir::TACOpcode::Mul:  return "i64.mul";
        case ir::TACOpcode::SDiv: return "i64.div_s";
        case ir::TACOpcode::UDiv: return "i64.div_u";
        case ir::TACOpcode::SRem: return "i64.rem_s";
        case ir::TACOpcode::URem: return "i64.rem_u";
        // Bitwise / shifts.
        case ir::TACOpcode::And:  return "i64.and";
        case ir::TACOpcode::Or:   return "i64.or";
        case ir::TACOpcode::Xor:  return "i64.xor";
        case ir::TACOpcode::Shl:  return "i64.shl";
        case ir::TACOpcode::LShr: return "i64.shr_u";
        case ir::TACOpcode::AShr: return "i64.shr_s";
        // Unary (pseudos; lowered in rewrite()).
        case ir::TACOpcode::Neg:  return "i64.neg";
        case ir::TACOpcode::Not:  return "i64.not";
        // Comparisons.
        case ir::TACOpcode::Eq:   return "i64.eq";
        case ir::TACOpcode::Ne:   return "i64.ne";
        case ir::TACOpcode::SLt:  return "i64.lt_s";
        case ir::TACOpcode::SLe:  return "i64.le_s";
        case ir::TACOpcode::SGt:  return "i64.gt_s";
        case ir::TACOpcode::SGe:  return "i64.ge_s";
        case ir::TACOpcode::ULt:  return "i64.lt_u";
        case ir::TACOpcode::ULe:  return "i64.le_u";
        case ir::TACOpcode::UGt:  return "i64.gt_u";
        case ir::TACOpcode::UGe:  return "i64.ge_u";
        // Data movement / control flow.
        case ir::TACOpcode::Const:      return "i64.const";
        case ir::TACOpcode::Move:       return "local.set";
        case ir::TACOpcode::Select:     return "select";
        case ir::TACOpcode::Phi:        return "phi";
        case ir::TACOpcode::Nop:        return "nop";
        case ir::TACOpcode::Call:       return "call";
        case ir::TACOpcode::Branch:     return "br";
        case ir::TACOpcode::CondBranch: return "br_if";
        default: return "jiterati." + ir::tac_opcode_str(op);
    }
}

} // namespace

WasmFunction select(ir::TACFunction const& function) {
    WasmFunction out;
    out.name = function.name;
    out.return_type = function.return_type;
    out.parameters = function.parameters;
    for (auto const& block : function.blocks) {
        WasmBlock wasm_block;
        wasm_block.label = block.label();
        for (auto const& instruction : block.instructions) {
            WasmInst inst;
            inst.comment = instruction.str();
            if (instruction.opcode == ir::TACOpcode::Return) {
                inst.opcode = "return";
                if (!instruction.operands.empty()) inst.operands = { instruction.operands[0].str() };
            } else {
                inst.opcode = opcode(instruction.opcode);
                if (instruction.result.has_value()) inst.operands.push_back(instruction.result->str());
                for (auto const& operand : instruction.operands) inst.operands.push_back(operand.str());
                for (auto target : instruction.targets) inst.operands.push_back("bb" + std::to_string(target));
                if (!instruction.symbol.empty()) inst.operands.push_back(instruction.symbol);
            }
            wasm_block.instructions.push_back(std::move(inst));
        }
        out.blocks.push_back(std::move(wasm_block));
    }
    return out;
}

} // namespace jiterati::be::wasm
