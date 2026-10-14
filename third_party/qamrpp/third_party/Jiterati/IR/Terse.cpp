#include "IR.hpp"

#include <map>
#include <stdexcept>

namespace jiterati::ir {
namespace {

class LoweringContext {
public:
    explicit LoweringContext(TACFunction& function)
        : function_(function), current_(function.add_block("entry").id) {
        for (std::uint32_t i = 0; i < function_.parameters.size(); ++i) {
            auto const& parameter = function_.parameters[i];
            bindings_[parameter.name] = TACValue::parameter(i, parameter.type, parameter.name);
        }
    }

    TACValue lower(TerseExpr const& expr) {
        switch (expr.op) {
            case TerseOp::Literal:
                return TACValue::constant_value(expr.type, expr.literal);
            case TerseOp::Argument:
                return lookup(expr.symbol);
            case TerseOp::Unary:
                return lower_unary(expr);
            case TerseOp::Binary:
                return lower_binary(expr);
            case TerseOp::Call:
                return lower_call(expr);
            case TerseOp::Let:
                return lower_let(expr);
            case TerseOp::If:
                return lower_if(expr);
        }
        return TACValue::invalid();
    }

    void emit_return(TerseExpr const& expr) {
        TACInstruction instruction;
        instruction.opcode = TACOpcode::Return;
        instruction.type = Type::void_ty();
        if (!expr.type.is_void()) instruction.operands.push_back(lower(expr));
        function_.append(current_, std::move(instruction));
    }

private:
    TACValue lookup(std::string const& name) const {
        auto it = bindings_.find(name);
        if (it == bindings_.end()) throw std::runtime_error("unknown terse symbol: " + name);
        return it->second;
    }

    TACValue emit_value(TACOpcode opcode, Type type, std::vector<TACValue> operands, std::string symbol = {}) {
        TACInstruction instruction;
        instruction.opcode = opcode;
        instruction.type = type;
        instruction.result = function_.make_temp(type);
        instruction.operands = std::move(operands);
        instruction.symbol = std::move(symbol);
        TACValue result = *instruction.result;
        function_.append(current_, std::move(instruction));
        return result;
    }

    TACValue lower_unary(TerseExpr const& expr) {
        if (expr.children.size() != 1) throw std::runtime_error("unary expression expects one operand");
        return emit_value(tac_opcode_for(expr.unary), expr.type, { lower(expr.children[0]) });
    }

    TACValue lower_binary(TerseExpr const& expr) {
        if (expr.children.size() != 2) throw std::runtime_error("binary expression expects two operands");
        return emit_value(tac_opcode_for(expr.binary), expr.type,
                          { lower(expr.children[0]), lower(expr.children[1]) });
    }

    TACValue lower_call(TerseExpr const& expr) {
        std::vector<TACValue> operands;
        operands.reserve(expr.children.size());
        for (auto const& child : expr.children) operands.push_back(lower(child));
        if (expr.type.is_void()) {
            TACInstruction instruction;
            instruction.opcode = TACOpcode::Call;
            instruction.type = Type::void_ty();
            instruction.symbol = expr.symbol;
            instruction.operands = std::move(operands);
            function_.append(current_, std::move(instruction));
            return TACValue::invalid();
        }
        return emit_value(TACOpcode::Call, expr.type, std::move(operands), expr.symbol);
    }

    TACValue lower_let(TerseExpr const& expr) {
        if (expr.children.size() != 2) throw std::runtime_error("let expression expects value and body");
        TACValue value = lower(expr.children[0]);
        auto previous = bindings_.find(expr.symbol);
        bool had_previous = previous != bindings_.end();
        TACValue previous_value = had_previous ? previous->second : TACValue::invalid();
        bindings_[expr.symbol] = value;
        TACValue result = lower(expr.children[1]);
        if (had_previous) {
            bindings_[expr.symbol] = previous_value;
        } else {
            bindings_.erase(expr.symbol);
        }
        return result;
    }

    TACValue lower_if(TerseExpr const& expr) {
        if (expr.children.size() != 3) throw std::runtime_error("if expression expects condition and two values");
        TACValue condition = lower(expr.children[0]);
        TACValue true_value = lower(expr.children[1]);
        TACValue false_value = lower(expr.children[2]);
        if (expr.type.is_void()) return TACValue::invalid();
        return emit_value(TACOpcode::Select, expr.type, { condition, true_value, false_value });
    }

    TACFunction& function_;
    BlockId current_ = 0;
    std::map<std::string, TACValue> bindings_;
};

} // namespace

TACFunction lower_to_tac(TerseFunction const& function) {
    TACFunction tac;
    tac.name = function.name;
    tac.return_type = function.return_type;
    tac.parameters = function.parameters;

    LoweringContext context(tac);
    context.emit_return(function.body);
    return tac;
}

TACModule lower_to_tac(TerseModule const& module) {
    TACModule tac;
    tac.name = module.name;
    tac.functions.reserve(module.functions.size());
    for (auto const& function : module.functions) tac.functions.push_back(lower_to_tac(function));
    return tac;
}

} // namespace jiterati::ir
