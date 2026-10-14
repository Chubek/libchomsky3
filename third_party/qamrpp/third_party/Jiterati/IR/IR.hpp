#ifndef JITERATI_IR_IR_HPP_INCLUDED
#define JITERATI_IR_IR_HPP_INCLUDED

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace jiterati::ir {

enum class ScalarType {
    Void,
    Bool,
    I32,
    I64,
    F32,
    F64,
    Ptr,
};

struct Type {
    ScalarType scalar = ScalarType::Void;

    static Type void_ty() { return { ScalarType::Void }; }
    static Type bool_ty() { return { ScalarType::Bool }; }
    static Type i32() { return { ScalarType::I32 }; }
    static Type i64() { return { ScalarType::I64 }; }
    static Type f32() { return { ScalarType::F32 }; }
    static Type f64() { return { ScalarType::F64 }; }
    static Type ptr() { return { ScalarType::Ptr }; }

    bool is_void() const { return scalar == ScalarType::Void; }
    bool is_integer() const;
    bool is_float() const;
    std::string str() const;

    friend bool operator==(Type lhs, Type rhs) { return lhs.scalar == rhs.scalar; }
    friend bool operator!=(Type lhs, Type rhs) { return !(lhs == rhs); }
};

using Immediate = std::variant<std::monostate, bool, std::int64_t, double, std::nullptr_t>;

std::string immediate_str(Immediate const& immediate);

struct Parameter {
    std::string name;
    Type type = Type::void_ty();
};

enum class TerseOp {
    Literal,
    Argument,
    Unary,
    Binary,
    Call,
    Let,
    If,
};

enum class UnaryOp {
    Neg,
    Not,
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    SDiv,
    UDiv,
    SRem,
    URem,
    And,
    Or,
    Xor,
    Shl,
    LShr,
    AShr,
    Eq,
    Ne,
    SLt,
    SLe,
    SGt,
    SGe,
    ULt,
    ULe,
    UGt,
    UGe,
};

std::string unary_op_str(UnaryOp op);
std::string binary_op_str(BinaryOp op);

struct TerseExpr {
    TerseOp op = TerseOp::Literal;
    Type type = Type::void_ty();
    Immediate literal;
    std::string symbol;
    UnaryOp unary = UnaryOp::Neg;
    BinaryOp binary = BinaryOp::Add;
    std::vector<TerseExpr> children;

    static TerseExpr literal_of(Type type, Immediate value);
    static TerseExpr argument(Type type, std::string name);
    static TerseExpr unary_expr(Type type, UnaryOp op, TerseExpr operand);
    static TerseExpr binary_expr(Type type, BinaryOp op, TerseExpr lhs, TerseExpr rhs);
    static TerseExpr call(Type type, std::string callee, std::vector<TerseExpr> args);
    static TerseExpr let(std::string name, TerseExpr value, TerseExpr body);
    static TerseExpr if_expr(Type type, TerseExpr condition, TerseExpr true_value, TerseExpr false_value);

    std::string str() const;
};

struct TerseFunction {
    std::string name;
    Type return_type = Type::void_ty();
    std::vector<Parameter> parameters;
    TerseExpr body;

    std::string str() const;
};

struct TerseModule {
    std::string name = "module";
    std::vector<TerseFunction> functions;

    TerseFunction& add_function(std::string name, Type return_type, std::vector<Parameter> parameters);
    std::string str() const;
};

using TempId = std::uint32_t;
using BlockId = std::uint32_t;

enum class TACOpcode {
    Nop,
    Const,
    Move,
    Add,
    Sub,
    Mul,
    SDiv,
    UDiv,
    SRem,
    URem,
    Neg,
    And,
    Or,
    Xor,
    Shl,
    LShr,
    AShr,
    Not,
    Eq,
    Ne,
    SLt,
    SLe,
    SGt,
    SGe,
    ULt,
    ULe,
    UGt,
    UGe,
    Select,
    Call,
    Return,
    Branch,
    CondBranch,
    Phi,
};

std::string tac_opcode_str(TACOpcode op);
TACOpcode tac_opcode_for(UnaryOp op);
TACOpcode tac_opcode_for(BinaryOp op);

struct TACValue {
    enum class Kind {
        Invalid,
        Temporary,
        Parameter,
        Constant,
    };

    Kind kind = Kind::Invalid;
    std::uint32_t index = 0;
    Type type = Type::void_ty();
    Immediate constant;
    std::string name;

    static TACValue invalid();
    static TACValue temporary(TempId id, Type type);
    static TACValue parameter(std::uint32_t index, Type type, std::string name = {});
    static TACValue constant_value(Type type, Immediate value);

    bool valid() const { return kind != Kind::Invalid; }
    std::string str() const;
};

struct TACInstruction {
    TACOpcode opcode = TACOpcode::Nop;
    Type type = Type::void_ty();
    std::optional<TACValue> result;
    std::vector<TACValue> operands;
    std::string symbol;
    std::vector<BlockId> targets;

    bool is_terminator() const;
    std::string str() const;
};

struct TACBlock {
    BlockId id = 0;
    std::string name;
    std::vector<TACInstruction> instructions;

    bool has_terminator() const;
    std::string label() const;
    std::string str() const;
};

struct TACFunction {
    std::string name;
    Type return_type = Type::void_ty();
    std::vector<Parameter> parameters;
    std::vector<TACBlock> blocks;

    TACBlock& add_block(std::string name = {});
    TACValue make_temp(Type type);
    TACInstruction& append(BlockId block, TACInstruction instruction);
    TACBlock* find_block(BlockId id);
    TACBlock const* find_block(BlockId id) const;
    TempId temp_count() const { return next_temp_; }
    std::string str() const;

private:
    TempId next_temp_ = 0;
};

struct TACModule {
    std::string name = "module";
    std::vector<TACFunction> functions;

    TACFunction& add_function(std::string name, Type return_type, std::vector<Parameter> parameters);
    TACFunction* find_function(std::string const& name);
    TACFunction const* find_function(std::string const& name) const;
    std::string str() const;
};

struct Diagnostic {
    enum class Severity {
        Note,
        Warning,
        Error,
    };

    Severity severity = Severity::Error;
    std::string message;
    std::string function;
    BlockId block = 0;
};

std::string diagnostic_str(Diagnostic const& diagnostic);

TACFunction lower_to_tac(TerseFunction const& function);
TACModule lower_to_tac(TerseModule const& module);

std::vector<Diagnostic> validate(TACFunction const& function);
std::vector<Diagnostic> validate(TACModule const& module);

void rewrite_values(TACFunction& function, TACValue const& from, TACValue const& to);
bool peephole(TACFunction& function);
bool peephole(TACModule& module);

std::optional<TerseModule> parse_terse_module(std::string const& source, std::vector<Diagnostic>* diagnostics = nullptr);

} // namespace jiterati::ir

#endif // JITERATI_IR_IR_HPP_INCLUDED
