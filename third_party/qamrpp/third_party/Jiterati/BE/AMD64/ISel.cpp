#include "ISel.hpp"

#include <sstream>

namespace jiterati::be::amd64 {
namespace {

std::string value(ir::TACValue const& v) {
    return v.str();
}

std::string opcode(ir::TACOpcode op) {
    switch (op) {
        case ir::TACOpcode::Add: return "add";
        case ir::TACOpcode::Sub: return "sub";
        case ir::TACOpcode::Mul: return "imul";
        case ir::TACOpcode::SDiv: return "idiv";
        case ir::TACOpcode::UDiv: return "div";
        case ir::TACOpcode::SRem: return "idiv.rem";
        case ir::TACOpcode::URem: return "div.rem";
        case ir::TACOpcode::And: return "and";
        case ir::TACOpcode::Or: return "or";
        case ir::TACOpcode::Xor: return "xor";
        case ir::TACOpcode::Shl: return "shl";
        case ir::TACOpcode::LShr: return "shr";
        case ir::TACOpcode::AShr: return "sar";
        case ir::TACOpcode::Neg: return "neg";
        case ir::TACOpcode::Not: return "not";
        case ir::TACOpcode::Eq: return "sete";
        case ir::TACOpcode::Ne: return "setne";
        case ir::TACOpcode::SLt: return "setl";
        case ir::TACOpcode::SLe: return "setle";
        case ir::TACOpcode::SGt: return "setg";
        case ir::TACOpcode::SGe: return "setge";
        case ir::TACOpcode::ULt: return "setb";
        case ir::TACOpcode::ULe: return "setbe";
        case ir::TACOpcode::UGt: return "seta";
        case ir::TACOpcode::UGe: return "setae";
        case ir::TACOpcode::Const:  return "mov";
        case ir::TACOpcode::Move: return "mov";
        case ir::TACOpcode::Select: return "select";
        case ir::TACOpcode::Phi: return "phi";
        case ir::TACOpcode::Nop: return "nop";
        case ir::TACOpcode::Call: return "call";
        case ir::TACOpcode::Branch: return "jmp";
        case ir::TACOpcode::CondBranch: return "jnz";
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
                if (!instruction.operands.empty()) inst.operands = { value(instruction.operands[0]) };
            } else if (instruction.result.has_value()) {
                inst.opcode = opcode(instruction.opcode);
                inst.operands.push_back(instruction.result->str());
                for (auto const& operand : instruction.operands) inst.operands.push_back(value(operand));
            } else {
                inst.opcode = opcode(instruction.opcode);
                for (auto const& operand : instruction.operands) inst.operands.push_back(value(operand));
                for (auto target : instruction.targets) inst.operands.push_back("bb" + std::to_string(target));
            }
            if (!instruction.symbol.empty()) inst.operands.push_back(instruction.symbol);
            machine_block.instructions.push_back(std::move(inst));
        }
        out.blocks.push_back(std::move(machine_block));
    }
    return out;
}

} // namespace jiterati::be::amd64
