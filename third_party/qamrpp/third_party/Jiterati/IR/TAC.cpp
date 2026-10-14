#include "IR.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace jiterati::ir {

std::string tac_opcode_str(TACOpcode op) {
    switch (op) {
        case TACOpcode::Nop: return "nop";
        case TACOpcode::Const: return "const";
        case TACOpcode::Move: return "mov";
        case TACOpcode::Add: return "add";
        case TACOpcode::Sub: return "sub";
        case TACOpcode::Mul: return "mul";
        case TACOpcode::SDiv: return "sdiv";
        case TACOpcode::UDiv: return "udiv";
        case TACOpcode::SRem: return "srem";
        case TACOpcode::URem: return "urem";
        case TACOpcode::Neg: return "neg";
        case TACOpcode::And: return "and";
        case TACOpcode::Or: return "or";
        case TACOpcode::Xor: return "xor";
        case TACOpcode::Shl: return "shl";
        case TACOpcode::LShr: return "lshr";
        case TACOpcode::AShr: return "ashr";
        case TACOpcode::Not: return "not";
        case TACOpcode::Eq: return "eq";
        case TACOpcode::Ne: return "ne";
        case TACOpcode::SLt: return "slt";
        case TACOpcode::SLe: return "sle";
        case TACOpcode::SGt: return "sgt";
        case TACOpcode::SGe: return "sge";
        case TACOpcode::ULt: return "ult";
        case TACOpcode::ULe: return "ule";
        case TACOpcode::UGt: return "ugt";
        case TACOpcode::UGe: return "uge";
        case TACOpcode::Select: return "select";
        case TACOpcode::Call: return "call";
        case TACOpcode::Return: return "ret";
        case TACOpcode::Branch: return "br";
        case TACOpcode::CondBranch: return "condbr";
        case TACOpcode::Phi: return "phi";
    }
    return "<?>";
}

TACOpcode tac_opcode_for(UnaryOp op) {
    switch (op) {
        case UnaryOp::Neg: return TACOpcode::Neg;
        case UnaryOp::Not: return TACOpcode::Not;
    }
    return TACOpcode::Nop;
}

TACOpcode tac_opcode_for(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return TACOpcode::Add;
        case BinaryOp::Sub: return TACOpcode::Sub;
        case BinaryOp::Mul: return TACOpcode::Mul;
        case BinaryOp::SDiv: return TACOpcode::SDiv;
        case BinaryOp::UDiv: return TACOpcode::UDiv;
        case BinaryOp::SRem: return TACOpcode::SRem;
        case BinaryOp::URem: return TACOpcode::URem;
        case BinaryOp::And: return TACOpcode::And;
        case BinaryOp::Or: return TACOpcode::Or;
        case BinaryOp::Xor: return TACOpcode::Xor;
        case BinaryOp::Shl: return TACOpcode::Shl;
        case BinaryOp::LShr: return TACOpcode::LShr;
        case BinaryOp::AShr: return TACOpcode::AShr;
        case BinaryOp::Eq: return TACOpcode::Eq;
        case BinaryOp::Ne: return TACOpcode::Ne;
        case BinaryOp::SLt: return TACOpcode::SLt;
        case BinaryOp::SLe: return TACOpcode::SLe;
        case BinaryOp::SGt: return TACOpcode::SGt;
        case BinaryOp::SGe: return TACOpcode::SGe;
        case BinaryOp::ULt: return TACOpcode::ULt;
        case BinaryOp::ULe: return TACOpcode::ULe;
        case BinaryOp::UGt: return TACOpcode::UGt;
        case BinaryOp::UGe: return TACOpcode::UGe;
    }
    return TACOpcode::Nop;
}

TACValue TACValue::invalid() {
    return {};
}

TACValue TACValue::temporary(TempId id, Type type) {
    TACValue value;
    value.kind = Kind::Temporary;
    value.index = id;
    value.type = type;
    return value;
}

TACValue TACValue::parameter(std::uint32_t index, Type type, std::string name) {
    TACValue value;
    value.kind = Kind::Parameter;
    value.index = index;
    value.type = type;
    value.name = std::move(name);
    return value;
}

TACValue TACValue::constant_value(Type type, Immediate immediate) {
    TACValue value;
    value.kind = Kind::Constant;
    value.type = type;
    value.constant = std::move(immediate);
    return value;
}

std::string TACValue::str() const {
    switch (kind) {
        case Kind::Invalid: return "<invalid>";
        case Kind::Temporary: return "%t" + std::to_string(index);
        case Kind::Parameter:
            if (!name.empty()) return "%" + name;
            return "%p" + std::to_string(index);
        case Kind::Constant: return immediate_str(constant);
    }
    return "<?>";
}

bool TACInstruction::is_terminator() const {
    return opcode == TACOpcode::Return || opcode == TACOpcode::Branch ||
           opcode == TACOpcode::CondBranch;
}

std::string TACInstruction::str() const {
    std::ostringstream out;
    out << "  ";
    if (result.has_value()) out << result->str() << " = ";
    out << tac_opcode_str(opcode);
    if (!symbol.empty()) out << " @" << symbol;
    for (auto const& operand : operands) out << ' ' << operand.str();
    for (auto target : targets) out << " bb" << target;
    if (!type.is_void()) out << " : " << type.str();
    return out.str();
}

bool TACBlock::has_terminator() const {
    return !instructions.empty() && instructions.back().is_terminator();
}

std::string TACBlock::label() const {
    if (!name.empty()) return name;
    return "bb" + std::to_string(id);
}

std::string TACBlock::str() const {
    std::ostringstream out;
    out << label() << ":\n";
    for (auto const& instruction : instructions) out << instruction.str() << "\n";
    return out.str();
}

TACBlock& TACFunction::add_block(std::string name) {
    BlockId id = static_cast<BlockId>(blocks.size());
    if (name.empty()) name = "bb" + std::to_string(id);
    blocks.push_back({ id, std::move(name), {} });
    return blocks.back();
}

TACValue TACFunction::make_temp(Type type) {
    return TACValue::temporary(next_temp_++, type);
}

TACInstruction& TACFunction::append(BlockId block, TACInstruction instruction) {
    TACBlock* target = find_block(block);
    if (target == nullptr) throw std::out_of_range("invalid TAC block id");
    target->instructions.push_back(std::move(instruction));
    return target->instructions.back();
}

TACBlock* TACFunction::find_block(BlockId id) {
    auto it = std::find_if(blocks.begin(), blocks.end(), [id](TACBlock const& block) {
        return block.id == id;
    });
    return it == blocks.end() ? nullptr : &*it;
}

TACBlock const* TACFunction::find_block(BlockId id) const {
    auto it = std::find_if(blocks.begin(), blocks.end(), [id](TACBlock const& block) {
        return block.id == id;
    });
    return it == blocks.end() ? nullptr : &*it;
}

std::string TACFunction::str() const {
    std::ostringstream out;
    out << "func @" << name << '(';
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (i != 0) out << ", ";
        out << '%' << parameters[i].name << ": " << parameters[i].type.str();
    }
    out << ") -> " << return_type.str() << "\n";
    for (auto const& block : blocks) out << block.str();
    return out.str();
}

TACFunction& TACModule::add_function(std::string name, Type return_type, std::vector<Parameter> parameters) {
    functions.emplace_back();
    TACFunction& function = functions.back();
    function.name = std::move(name);
    function.return_type = return_type;
    function.parameters = std::move(parameters);
    return functions.back();
}

TACFunction* TACModule::find_function(std::string const& name) {
    auto it = std::find_if(functions.begin(), functions.end(), [&](TACFunction const& function) {
        return function.name == name;
    });
    return it == functions.end() ? nullptr : &*it;
}

TACFunction const* TACModule::find_function(std::string const& name) const {
    auto it = std::find_if(functions.begin(), functions.end(), [&](TACFunction const& function) {
        return function.name == name;
    });
    return it == functions.end() ? nullptr : &*it;
}

std::string TACModule::str() const {
    std::ostringstream out;
    out << "tac module " << name << "\n";
    for (auto const& function : functions) out << function.str() << "\n";
    return out.str();
}

} // namespace jiterati::ir
