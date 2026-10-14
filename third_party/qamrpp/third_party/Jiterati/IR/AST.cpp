#include "IR.hpp"

#include <sstream>

namespace jiterati::ir {

bool Type::is_integer() const {
    return scalar == ScalarType::Bool || scalar == ScalarType::I32 ||
           scalar == ScalarType::I64 || scalar == ScalarType::Ptr;
}

bool Type::is_float() const {
    return scalar == ScalarType::F32 || scalar == ScalarType::F64;
}

std::string Type::str() const {
    switch (scalar) {
        case ScalarType::Void: return "void";
        case ScalarType::Bool: return "bool";
        case ScalarType::I32: return "i32";
        case ScalarType::I64: return "i64";
        case ScalarType::F32: return "f32";
        case ScalarType::F64: return "f64";
        case ScalarType::Ptr: return "ptr";
    }
    return "<?>";
}

std::string immediate_str(Immediate const& immediate) {
    std::ostringstream out;
    std::visit([&](auto const& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, std::monostate>) {
            out << "undef";
        } else if constexpr (std::is_same_v<Value, bool>) {
            out << (value ? "true" : "false");
        } else if constexpr (std::is_same_v<Value, std::nullptr_t>) {
            out << "null";
        } else {
            out << value;
        }
    }, immediate);
    return out.str();
}

std::string unary_op_str(UnaryOp op) {
    switch (op) {
        case UnaryOp::Neg: return "neg";
        case UnaryOp::Not: return "not";
    }
    return "<?>";
}

std::string binary_op_str(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::SDiv: return "s/";
        case BinaryOp::UDiv: return "u/";
        case BinaryOp::SRem: return "s%";
        case BinaryOp::URem: return "u%";
        case BinaryOp::And: return "&";
        case BinaryOp::Or: return "|";
        case BinaryOp::Xor: return "^";
        case BinaryOp::Shl: return "<<";
        case BinaryOp::LShr: return "l>>";
        case BinaryOp::AShr: return "a>>";
        case BinaryOp::Eq: return "==";
        case BinaryOp::Ne: return "!=";
        case BinaryOp::SLt: return "s<";
        case BinaryOp::SLe: return "s<=";
        case BinaryOp::SGt: return "s>";
        case BinaryOp::SGe: return "s>=";
        case BinaryOp::ULt: return "u<";
        case BinaryOp::ULe: return "u<=";
        case BinaryOp::UGt: return "u>";
        case BinaryOp::UGe: return "u>=";
    }
    return "<?>";
}

TerseExpr TerseExpr::literal_of(Type type, Immediate value) {
    TerseExpr expr;
    expr.op = TerseOp::Literal;
    expr.type = type;
    expr.literal = std::move(value);
    return expr;
}

TerseExpr TerseExpr::argument(Type type, std::string name) {
    TerseExpr expr;
    expr.op = TerseOp::Argument;
    expr.type = type;
    expr.symbol = std::move(name);
    return expr;
}

TerseExpr TerseExpr::unary_expr(Type type, UnaryOp op, TerseExpr operand) {
    TerseExpr expr;
    expr.op = TerseOp::Unary;
    expr.type = type;
    expr.unary = op;
    expr.children.push_back(std::move(operand));
    return expr;
}

TerseExpr TerseExpr::binary_expr(Type type, BinaryOp op, TerseExpr lhs, TerseExpr rhs) {
    TerseExpr expr;
    expr.op = TerseOp::Binary;
    expr.type = type;
    expr.binary = op;
    expr.children.push_back(std::move(lhs));
    expr.children.push_back(std::move(rhs));
    return expr;
}

TerseExpr TerseExpr::call(Type type, std::string callee, std::vector<TerseExpr> args) {
    TerseExpr expr;
    expr.op = TerseOp::Call;
    expr.type = type;
    expr.symbol = std::move(callee);
    expr.children = std::move(args);
    return expr;
}

TerseExpr TerseExpr::let(std::string name, TerseExpr value, TerseExpr body) {
    TerseExpr expr;
    expr.op = TerseOp::Let;
    expr.type = body.type;
    expr.symbol = std::move(name);
    expr.children.push_back(std::move(value));
    expr.children.push_back(std::move(body));
    return expr;
}

TerseExpr TerseExpr::if_expr(Type type, TerseExpr condition, TerseExpr true_value, TerseExpr false_value) {
    TerseExpr expr;
    expr.op = TerseOp::If;
    expr.type = type;
    expr.children.push_back(std::move(condition));
    expr.children.push_back(std::move(true_value));
    expr.children.push_back(std::move(false_value));
    return expr;
}

std::string TerseExpr::str() const {
    std::ostringstream out;
    switch (op) {
        case TerseOp::Literal:
            out << immediate_str(literal);
            break;
        case TerseOp::Argument:
            out << symbol;
            break;
        case TerseOp::Unary:
            out << '(' << unary_op_str(unary) << ' ' << children.at(0).str() << ')';
            break;
        case TerseOp::Binary:
            out << '(' << binary_op_str(binary) << ' ' << children.at(0).str()
                << ' ' << children.at(1).str() << ')';
            break;
        case TerseOp::Call:
            out << '(' << symbol;
            for (auto const& child : children) out << ' ' << child.str();
            out << ')';
            break;
        case TerseOp::Let:
            out << "(let " << symbol << ' ' << children.at(0).str() << ' '
                << children.at(1).str() << ')';
            break;
        case TerseOp::If:
            out << "(if " << children.at(0).str() << ' ' << children.at(1).str()
                << ' ' << children.at(2).str() << ')';
            break;
    }
    return out.str();
}

std::string TerseFunction::str() const {
    std::ostringstream out;
    out << "fn " << name << '(';
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (i != 0) out << ", ";
        out << parameters[i].name << ": " << parameters[i].type.str();
    }
    out << ") -> " << return_type.str() << " = " << body.str();
    return out.str();
}

TerseFunction& TerseModule::add_function(std::string name, Type return_type, std::vector<Parameter> parameters) {
    functions.push_back({ std::move(name), return_type, std::move(parameters), {} });
    return functions.back();
}

std::string TerseModule::str() const {
    std::ostringstream out;
    out << "terse module " << name << "\n";
    for (auto const& function : functions) out << function.str() << "\n";
    return out.str();
}

} // namespace jiterati::ir
