/** @file Jiterati.hpp
 *  @brief Header-only core JIT library for Jiterati.
 *
 *  Defines the IR model (Module, Function, Block, Value, Instruction),
 *  a fluent C++ DSL for building functions, and the JIT compilation entry point.
 *  This file is intentionally header-only and depends only on the C++17 standard
 *  library.
 */
#ifndef JITERATI_HPP_INCLUDED
#define JITERATI_HPP_INCLUDED

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <variant>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <list>
#include <sstream>
#include <type_traits>
#include <stdexcept>

namespace jiterati {

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
class Type;
class Value;
class Instruction;
class Block;
class Function;
class Module;
class Backend;
class CompiledFunction;

// -----------------------------------------------------------------------------
// Types
// -----------------------------------------------------------------------------

/** Primitive value types in Jiterati IR. */
enum class PrimType {
    Void,
    I1,
    I8,
    I16,
    I32,
    I64,
    F32,
    F64,
    Ptr,
};

/** A simple type wrapper. For now only primitive types are supported. */
class Type {
public:
    Type() = default;
    explicit Type(PrimType p) : prim_(p) {}

    static Type void_ty() { return Type(PrimType::Void); }
    static Type i1()      { return Type(PrimType::I1); }
    static Type i8()      { return Type(PrimType::I8); }
    static Type i16()     { return Type(PrimType::I16); }
    static Type i32()     { return Type(PrimType::I32); }
    static Type i64()     { return Type(PrimType::I64); }
    static Type f32()     { return Type(PrimType::F32); }
    static Type f64()     { return Type(PrimType::F64); }
    static Type ptr()     { return Type(PrimType::Ptr); }

    PrimType prim() const { return prim_; }

    bool is_void() const { return prim_ == PrimType::Void; }
    bool is_int() const {
        return prim_ == PrimType::I1 || prim_ == PrimType::I8 ||
               prim_ == PrimType::I16 || prim_ == PrimType::I32 || prim_ == PrimType::I64;
    }
    bool is_float() const { return prim_ == PrimType::F32 || prim_ == PrimType::F64; }
    bool is_pointer() const { return prim_ == PrimType::Ptr; }

    std::size_t size() const {
        switch (prim_) {
            case PrimType::Void: return 0;
            case PrimType::I1:   return 1;
            case PrimType::I8:   return 1;
            case PrimType::I16:  return 2;
            case PrimType::I32:  return 4;
            case PrimType::I64:  return 8;
            case PrimType::F32:  return 4;
            case PrimType::F64:  return 8;
            case PrimType::Ptr:  return sizeof(void*);
        }
        return 0;
    }

    bool operator==(Type const& other) const { return prim_ == other.prim_; }
    bool operator!=(Type const& other) const { return prim_ != other.prim_; }

    std::string to_string() const {
        switch (prim_) {
            case PrimType::Void: return "void";
            case PrimType::I1:   return "i1";
            case PrimType::I8:   return "i8";
            case PrimType::I16:  return "i16";
            case PrimType::I32:  return "i32";
            case PrimType::I64:  return "i64";
            case PrimType::F32:  return "f32";
            case PrimType::F64:  return "f64";
            case PrimType::Ptr:  return "ptr";
        }
        return "?";
    }

private:
    PrimType prim_ = PrimType::Void;
};

// -----------------------------------------------------------------------------
// Values and constants
// -----------------------------------------------------------------------------

/** A handle to an SSA value.  The value may be a constant, an instruction
 *  result, or a function parameter.  Indices use a biased representation so
 *  that zero can be reserved and negative values can encode constants.
 *
 *   0              : invalid / none
 *   1..            : virtual register / instruction result
 *   -1..-INT32_MAX : constant pool index (offset by +1)
 */
class Value {
public:
    Value() = default;
    explicit Value(int32_t idx) : idx_(idx) {}

    static Value none() { return Value(0); }
    static Value vreg(uint32_t idx) { return Value(static_cast<int32_t>(idx + 1)); }
    static Value const_idx(uint32_t pool_idx) {
        assert(pool_idx < 0x7FFFFFFFu);
        return Value(-static_cast<int32_t>(pool_idx + 1));
    }

    bool valid() const { return idx_ != 0; }
    bool is_vreg() const { return idx_ > 0; }
    bool is_const() const { return idx_ < 0; }

    uint32_t vreg_index() const {
        assert(is_vreg());
        return static_cast<uint32_t>(idx_ - 1);
    }
    uint32_t const_index() const {
        assert(is_const());
        return static_cast<uint32_t>(-idx_ - 1);
    }

    int32_t raw() const { return idx_; }

    bool operator==(Value const& o) const { return idx_ == o.idx_; }
    bool operator!=(Value const& o) const { return idx_ != o.idx_; }

    std::string to_string() const {
        if (is_vreg()) return "%" + std::to_string(vreg_index());
        if (is_const()) return "$c" + std::to_string(const_index());
        return "%<none>";
    }

private:
    int32_t idx_ = 0;
};

// -----------------------------------------------------------------------------
// Opcodes
// -----------------------------------------------------------------------------

/** Opcodes for three-address Jiterati IR instructions. */
enum class Opcode : uint16_t {
    // Terminators
    Ret = 1,
    Br,
    CondBr,
    Switch,
    Call,
    Unreachable,

    // Memory
    Load,
    Store,
    Alloca,
    GetParam,

    // Arithmetic (integer)
    Add,
    Sub,
    Mul,
    SDiv,
    SRem,
    UDiv,
    URem,
    Neg,

    // Bitwise
    And,
    Or,
    Xor,
    Shl,
    LShr,
    AShr,
    Not,

    // Comparisons
    ICmpEq,
    ICmpNe,
    ICmpSLt,
    ICmpSLe,
    ICmpSGt,
    ICmpSGe,
    ICmpULt,
    ICmpULe,
    ICmpUGt,
    ICmpUGe,

    // Conversions
    Trunc,
    ZExt,
    SExt,
    IntToPtr,
    PtrToInt,

    // Other
    Phi,
    Select,
    Copy,
};

inline bool opcode_is_terminator(Opcode op) {
    switch (op) {
        case Opcode::Ret:
        case Opcode::Br:
        case Opcode::CondBr:
        case Opcode::Switch:
        case Opcode::Call:
        case Opcode::Unreachable:
            return true;
        default:
            return false;
    }
}

inline bool opcode_is_commutative(Opcode op) {
    switch (op) {
        case Opcode::Add:
        case Opcode::Mul:
        case Opcode::And:
        case Opcode::Or:
        case Opcode::Xor:
        case Opcode::ICmpEq:
        case Opcode::ICmpNe:
            return true;
        default:
            return false;
    }
}

inline std::string opcode_name(Opcode op) {
    switch (op) {
        case Opcode::Ret:        return "ret";
        case Opcode::Br:         return "br";
        case Opcode::CondBr:     return "condbr";
        case Opcode::Switch:     return "switch";
        case Opcode::Call:       return "call";
        case Opcode::Unreachable:return "unreachable";
        case Opcode::Load:       return "load";
        case Opcode::Store:      return "store";
        case Opcode::Alloca:     return "alloca";
        case Opcode::GetParam:   return "param";
        case Opcode::Add:        return "add";
        case Opcode::Sub:        return "sub";
        case Opcode::Mul:        return "mul";
        case Opcode::SDiv:       return "sdiv";
        case Opcode::SRem:       return "srem";
        case Opcode::UDiv:       return "udiv";
        case Opcode::URem:       return "urem";
        case Opcode::Neg:        return "neg";
        case Opcode::And:        return "and";
        case Opcode::Or:         return "or";
        case Opcode::Xor:        return "xor";
        case Opcode::Shl:        return "shl";
        case Opcode::LShr:       return "lshr";
        case Opcode::AShr:       return "ashr";
        case Opcode::Not:        return "not";
        case Opcode::ICmpEq:     return "icmp_eq";
        case Opcode::ICmpNe:     return "icmp_ne";
        case Opcode::ICmpSLt:    return "icmp_slt";
        case Opcode::ICmpSLe:    return "icmp_sle";
        case Opcode::ICmpSGt:    return "icmp_sgt";
        case Opcode::ICmpSGe:    return "icmp_sge";
        case Opcode::ICmpULt:    return "icmp_ult";
        case Opcode::ICmpULe:    return "icmp_ule";
        case Opcode::ICmpUGt:    return "icmp_ugt";
        case Opcode::ICmpUGe:    return "icmp_uge";
        case Opcode::Trunc:      return "trunc";
        case Opcode::ZExt:       return "zext";
        case Opcode::SExt:       return "sext";
        case Opcode::IntToPtr:   return "inttoptr";
        case Opcode::PtrToInt:   return "ptrtoint";
        case Opcode::Phi:        return "phi";
        case Opcode::Select:     return "select";
        case Opcode::Copy:       return "copy";
    }
    return "<?>";
}

// -----------------------------------------------------------------------------
// Constant pool
// -----------------------------------------------------------------------------

/** Supported constant kinds. */
using Constant = std::variant<
    std::monostate,
    bool,
    int8_t,
    uint8_t,
    int16_t,
    uint16_t,
    int32_t,
    uint32_t,
    int64_t,
    uint64_t,
    float,
    double,
    std::nullptr_t
>;

inline std::string constant_to_string(Constant const& c, Type ty) {
    std::ostringstream oss;
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            oss << "undef";
        } else if constexpr (std::is_same_v<T, bool>) {
            oss << (v ? "true" : "false");
        } else if constexpr (std::is_same_v<T, float>) {
            oss << v << "f";
        } else if constexpr (std::is_same_v<T, double>) {
            oss << v;
        } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
            oss << "null";
        } else {
            if (ty.prim() == PrimType::I8 || ty.prim() == PrimType::I16 ||
                ty.prim() == PrimType::I32 || ty.prim() == PrimType::I64) {
                oss << static_cast<int64_t>(v);
            } else {
                oss << v;
            }
        }
    }, c);
    return oss.str();
}

// -----------------------------------------------------------------------------
// Instruction
// -----------------------------------------------------------------------------

/** A single IR instruction.  Instructions live in a Function and are indexed by
 *  their position in the function's instruction vector. */
class Instruction {
public:
    Instruction() = default;
    Instruction(uint32_t id, Opcode op, Type ty, std::vector<Value> operands = {})
        : id_(id), opcode_(op), type_(ty), operands_(std::move(operands)) {}

    uint32_t id() const { return id_; }
    Opcode opcode() const { return opcode_; }
    Type type() const { return type_; }

    std::vector<Value> const& operands() const { return operands_; }
    std::vector<Value>& operands() { return operands_; }

    Value operand(std::size_t i) const { return operands_.at(i); }
    void set_operand(std::size_t i, Value v) { operands_.at(i) = v; }

    std::string const& name() const { return name_; }
    void set_name(std::string n) { name_ = std::move(n); }

    Block* parent() const { return parent_; }
    void set_parent(Block* b) { parent_ = b; }

    bool is_terminator() const { return opcode_is_terminator(opcode_); }
    bool is_commutative() const { return opcode_is_commutative(opcode_); }

    std::string to_string() const;

private:
    uint32_t id_ = 0;
    Opcode opcode_ = Opcode::Copy;
    Type type_ = Type::void_ty();
    std::vector<Value> operands_;
    std::string name_;
    Block* parent_ = nullptr;
};

// -----------------------------------------------------------------------------
// Block
// -----------------------------------------------------------------------------

/** A basic block owns a sequence of instructions and has successor/predecessor
 *  edges recorded in the function. */
class Block {
public:
    explicit Block(Function* fn, std::string name)
        : function_(fn), name_(std::move(name)) {}

    Function* function() const { return function_; }
    std::string const& name() const { return name_; }

    std::vector<Instruction*> const& instructions() const { return instrs_; }
    std::vector<Instruction*>& instructions() { return instrs_; }

    Instruction* append(Instruction* inst);

    // DSL builders -----------------------------------------------------------
    Value arg(std::size_t idx) const;

    Value make_value(uint32_t inst_id) const;
    Instruction* make_instruction(Opcode op, Type ty, std::vector<Value> operands);
    Value build_instruction(Opcode op, Type ty, std::vector<Value> operands);

    // Terminators
    void ret(Value v);
    void ret_void();
    void br(Block* target);
    void cond_br(Value cond, Block* true_target, Block* false_target);

    // Memory
    Value load(Type ty, Value ptr);
    void store(Value ptr, Value val);
    Value alloca(Type ty);

    // Arithmetic
    Value add(Value a, Value b);
    Value sub(Value a, Value b);
    Value mul(Value a, Value b);
    Value sdiv(Value a, Value b);
    Value srem(Value a, Value b);
    Value udiv(Value a, Value b);
    Value urem(Value a, Value b);
    Value neg(Value a);

    // Bitwise
    Value bitwise_and(Value a, Value b);
    Value bitwise_or (Value a, Value b);
    Value bitwise_xor(Value a, Value b);
    Value shl(Value a, Value b);
    Value lshr(Value a, Value b);
    Value ashr(Value a, Value b);
    Value bitwise_not(Value a);

    // Comparisons
    Value icmp_eq(Value a, Value b);
    Value icmp_ne(Value a, Value b);
    Value icmp_slt(Value a, Value b);
    Value icmp_sle(Value a, Value b);
    Value icmp_sgt(Value a, Value b);
    Value icmp_sge(Value a, Value b);

    // Conversions
    Value trunc(Type dst, Value a);
    Value zext(Type dst, Value a);
    Value sext(Type dst, Value a);
    Value inttoptr(Value a);
    Value ptrtoint(Type dst, Value a);

    // Other
    Value select(Value cond, Value a, Value b);
    Value call(Function* callee, std::vector<Value> args);
    Value phi(Type ty, std::vector<std::pair<Block*, Value>> const& preds);

    // CFG helpers
    void add_successor(Block* succ);
    void add_predecessor(Block* pred);
    std::vector<Block*> const& successors() const { return successors_; }
    std::vector<Block*> const& predecessors() const { return predecessors_; }

private:
    Function* function_ = nullptr;
    std::string name_;
    std::vector<Instruction*> instrs_;
    std::vector<Block*> successors_;
    std::vector<Block*> predecessors_;
};

// -----------------------------------------------------------------------------
// Function
// -----------------------------------------------------------------------------

/** A function consists of parameters, named blocks, and a constant pool. */
class Function {
public:
    Function(Module* mod, std::string name, Type ret_ty, std::vector<Type> params)
        : module_(mod), name_(std::move(name)), ret_type_(ret_ty), param_types_(std::move(params)) {
        next_inst_id_ = static_cast<uint32_t>(param_types_.size()) + 1;
    }

    Module* module() const { return module_; }
    std::string const& name() const { return name_; }
    Type ret_type() const { return ret_type_; }
    std::vector<Type> const& param_types() const { return param_types_; }

    Block* create_block(std::string name);
    Block* entry_block() const { return blocks_.empty() ? nullptr : blocks_.front().get(); }

    std::vector<std::unique_ptr<Block>> const& blocks() const { return blocks_; }
    std::vector<std::unique_ptr<Block>>& blocks() { return blocks_; }
    std::list<Instruction>& instr_storage() { return instructions_; }
    std::list<Instruction> const& instr_storage() const { return instructions_; }

    Instruction* make_instruction(Opcode op, Type ty, std::vector<Value> operands);
    Instruction* make_constant(Constant c, Type ty);
    Constant const_value(uint32_t idx) const { return constants_.at(idx); }
    Type const_type(uint32_t idx) const { return const_types_.at(idx); }
    std::size_t const_count() const { return constants_.size(); }
    Type value_type(Value v) const;

    std::size_t param_count() const { return param_types_.size(); }
    Value param_value(std::size_t idx) const {
        assert(idx < param_types_.size());
        return Value::vreg(static_cast<uint32_t>(idx));
    }

    // Constant helpers
    Value const_i32(int32_t v);
    Value const_i64(int64_t v);
    Value const_bool(bool v);
    Value const_ptr(std::nullptr_t);

    Instruction* instruction_by_id(uint32_t id);

    std::string to_string() const;

private:
    Module* module_ = nullptr;
    std::string name_;
    Type ret_type_;
    std::vector<Type> param_types_;
    std::vector<std::unique_ptr<Block>> blocks_;
    std::list<Instruction> instructions_;
    std::vector<Constant> constants_;
    std::vector<Type> const_types_;
    uint32_t next_inst_id_ = 1;
};

// -----------------------------------------------------------------------------
// Module
// -----------------------------------------------------------------------------

/** A module owns functions and may later own global data. */
class Module {
public:
    explicit Module(std::string name = "module") : name_(std::move(name)) {}

    std::string const& name() const { return name_; }

    Function* create_function(std::string name, Type ret_ty, std::vector<Type> params) {
        functions_.emplace_back(std::make_unique<Function>(this, std::move(name), ret_ty, std::move(params)));
        return functions_.back().get();
    }

    template <typename Sig>
    Function* create_function(std::string name);

    std::vector<std::unique_ptr<Function>> const& functions() const { return functions_; }
    std::vector<std::unique_ptr<Function>>& functions() { return functions_; }

    Function* find_function(std::string_view name) const {
        for (auto const& f : functions_) {
            if (f->name() == name) return f.get();
        }
        return nullptr;
    }

    std::string to_string() const;

private:
    std::string name_;
    std::vector<std::unique_ptr<Function>> functions_;
};

// -----------------------------------------------------------------------------
// Function signature helper
// -----------------------------------------------------------------------------

template <typename T> struct TypeOf;

template <> struct TypeOf<void>    { static Type get() { return Type::void_ty(); } };
template <> struct TypeOf<bool>    { static Type get() { return Type::i1(); } };
template <> struct TypeOf<int8_t>  { static Type get() { return Type::i8(); } };
template <> struct TypeOf<uint8_t> { static Type get() { return Type::i8(); } };
template <> struct TypeOf<int16_t> { static Type get() { return Type::i16(); } };
template <> struct TypeOf<uint16_t>{ static Type get() { return Type::i16(); } };
template <> struct TypeOf<int32_t> { static Type get() { return Type::i32(); } };
template <> struct TypeOf<uint32_t>{ static Type get() { return Type::i32(); } };
template <> struct TypeOf<int64_t> { static Type get() { return Type::i64(); } };
template <> struct TypeOf<uint64_t>{ static Type get() { return Type::i64(); } };
template <> struct TypeOf<float>   { static Type get() { return Type::f32(); } };
template <> struct TypeOf<double>  { static Type get() { return Type::f64(); } };
template <> struct TypeOf<void*>   { static Type get() { return Type::ptr(); } };
template <typename T> struct TypeOf<T*> { static Type get() { return Type::ptr(); } };

template <typename T> struct SignatureOf;

template <typename Ret, typename... Args>
struct SignatureOf<Ret(Args...)> {
    static Type ret() { return TypeOf<Ret>::get(); }
    static std::vector<Type> params() { return { TypeOf<Args>::get()... }; }
};

template <typename Sig>
Function* Module::create_function(std::string name) {
    return create_function(std::move(name), SignatureOf<Sig>::ret(), SignatureOf<Sig>::params());
}

// -----------------------------------------------------------------------------
// JIT entry point
// -----------------------------------------------------------------------------

/** The JIT class orchestrates pass pipelines and backend compilation. */
class JIT {
public:
    static JIT create_default();

    /** Compile a single function to executable code. */
    std::unique_ptr<CompiledFunction> compile(Function& fn, Backend& backend);
};

std::string print_jbl(Module const& module);
std::unique_ptr<Module> parse_jbl(std::string_view source, std::string* error = nullptr);

// -----------------------------------------------------------------------------
// Implementation details
// -----------------------------------------------------------------------------

inline std::string Instruction::to_string() const {
    std::ostringstream oss;
    if (type_.is_void()) {
        oss << "  " << opcode_name(opcode_);
    } else {
        oss << "  " << (name_.empty() ? ("%" + std::to_string(id_)) : name_)
            << " = " << opcode_name(opcode_);
    }
    auto format_operand = [this](Value const& op) {
        if (op.is_const() && parent_ != nullptr && parent_->function() != nullptr) {
            Function const* function = parent_->function();
            return constant_to_string(function->const_value(op.const_index()), function->const_type(op.const_index()));
        }
        return op.to_string();
    };
    for (auto const& op : operands_) {
        oss << " " << format_operand(op);
    }
    if (type_.is_void()) return oss.str();
    oss << " : " << type_.to_string();
    return oss.str();
}

inline Instruction* Block::append(Instruction* inst) {
    inst->set_parent(this);
    instructions().push_back(inst);
    return inst;
}

inline Value Block::arg(std::size_t idx) const {
    return function_->param_value(idx);
}

inline Value Block::make_value(uint32_t inst_id) const {
    (void)this;
    return Value::vreg(inst_id);
}

inline Instruction* Block::make_instruction(Opcode op, Type ty, std::vector<Value> operands) {
    auto* inst = function_->make_instruction(op, ty, std::move(operands));
    append(inst);
    return inst;
}

inline Value Block::build_instruction(Opcode op, Type ty, std::vector<Value> operands) {
    auto* inst = make_instruction(op, ty, std::move(operands));
    return make_value(inst->id());
}

inline Type infer_value_type(Function const* function, Value value, Type fallback = Type::i32()) {
    if (function == nullptr || !value.valid()) return fallback;
    return function->value_type(value);
}

inline void Block::add_successor(Block* succ) {
    if (std::find(successors_.begin(), successors_.end(), succ) == successors_.end())
        successors_.push_back(succ);
}

inline void Block::add_predecessor(Block* pred) {
    if (std::find(predecessors_.begin(), predecessors_.end(), pred) == predecessors_.end())
        predecessors_.push_back(pred);
}

// Terminators
inline void Block::ret(Value v) {
    make_instruction(Opcode::Ret, Type::void_ty(), { v });
}
inline void Block::ret_void() {
    make_instruction(Opcode::Ret, Type::void_ty(), {});
}
inline void Block::br(Block* target) {
    make_instruction(Opcode::Br, Type::void_ty(), { Value::const_idx(0) });
    add_successor(target);
    target->add_predecessor(this);
}
inline void Block::cond_br(Value cond, Block* true_target, Block* false_target) {
    make_instruction(Opcode::CondBr, Type::void_ty(), { cond });
    add_successor(true_target);
    add_successor(false_target);
    true_target->add_predecessor(this);
    false_target->add_predecessor(this);
}

// Memory
inline Value Block::load(Type ty, Value ptr) {
    return build_instruction(Opcode::Load, ty, { ptr });
}
inline void Block::store(Value ptr, Value val) {
    make_instruction(Opcode::Store, Type::void_ty(), { ptr, val });
}
inline Value Block::alloca(Type ty) {
    (void)ty;
    return build_instruction(Opcode::Alloca, Type::ptr(), { Value::const_idx(0) });
}

// Arithmetic
#define JITERATI_BINARY_OP(NAME, OPCODE)                                      \
    inline Value Block::NAME(Value a, Value b) {                              \
        Type ty = infer_value_type(function_, a, infer_value_type(function_, b, Type::i32())); \
        return build_instruction(OPCODE, ty, { a, b }); \
    }
JITERATI_BINARY_OP(add, Opcode::Add)
JITERATI_BINARY_OP(sub, Opcode::Sub)
JITERATI_BINARY_OP(mul, Opcode::Mul)
JITERATI_BINARY_OP(sdiv, Opcode::SDiv)
JITERATI_BINARY_OP(srem, Opcode::SRem)
JITERATI_BINARY_OP(udiv, Opcode::UDiv)
JITERATI_BINARY_OP(urem, Opcode::URem)
#undef JITERATI_BINARY_OP

inline Value Block::neg(Value a) {
    return build_instruction(Opcode::Neg, infer_value_type(function_, a), { a });
}

// Bitwise
#define JITERATI_BIT_OP(NAME, OPCODE)                                         \
    inline Value Block::NAME(Value a, Value b) {                              \
        Type ty = infer_value_type(function_, a, infer_value_type(function_, b, Type::i32())); \
        return build_instruction(OPCODE, ty, { a, b });              \
    }
JITERATI_BIT_OP(bitwise_and, Opcode::And)
JITERATI_BIT_OP(bitwise_or,  Opcode::Or)
JITERATI_BIT_OP(bitwise_xor, Opcode::Xor)
JITERATI_BIT_OP(shl, Opcode::Shl)
JITERATI_BIT_OP(lshr, Opcode::LShr)
JITERATI_BIT_OP(ashr, Opcode::AShr)
#undef JITERATI_BIT_OP

inline Value Block::bitwise_not(Value a) {
    return build_instruction(Opcode::Not, infer_value_type(function_, a), { a });
}

// Comparisons
#define JITERATI_CMP_OP(NAME, OPCODE)                                         \
    inline Value Block::NAME(Value a, Value b) {                              \
        return build_instruction(OPCODE, Type::i1(), { a, b });               \
    }
JITERATI_CMP_OP(icmp_eq, Opcode::ICmpEq)
JITERATI_CMP_OP(icmp_ne, Opcode::ICmpNe)
JITERATI_CMP_OP(icmp_slt, Opcode::ICmpSLt)
JITERATI_CMP_OP(icmp_sle, Opcode::ICmpSLe)
JITERATI_CMP_OP(icmp_sgt, Opcode::ICmpSGt)
JITERATI_CMP_OP(icmp_sge, Opcode::ICmpSGe)
#undef JITERATI_CMP_OP

// Conversions
inline Value Block::trunc(Type dst, Value a) {
    return build_instruction(Opcode::Trunc, dst, { a });
}
inline Value Block::zext(Type dst, Value a) {
    return build_instruction(Opcode::ZExt, dst, { a });
}
inline Value Block::sext(Type dst, Value a) {
    return build_instruction(Opcode::SExt, dst, { a });
}
inline Value Block::inttoptr(Value a) {
    return build_instruction(Opcode::IntToPtr, Type::ptr(), { a });
}
inline Value Block::ptrtoint(Type dst, Value a) {
    return build_instruction(Opcode::PtrToInt, dst, { a });
}

// Other
inline Value Block::select(Value cond, Value a, Value b) {
    Type ty = infer_value_type(function_, a, infer_value_type(function_, b, Type::i32()));
    return build_instruction(Opcode::Select, ty, { cond, a, b });
}
inline Value Block::call(Function* callee, std::vector<Value> args) {
    args.insert(args.begin(), Value::vreg(0));
    auto* inst = make_instruction(Opcode::Call, callee->ret_type(), std::move(args));
    inst->set_name(callee->name());
    return make_value(inst->id());
}
inline Value Block::phi(Type ty, std::vector<std::pair<Block*, Value>> const& preds) {
    std::vector<Value> ops;
    for (auto const& p : preds) {
        ops.push_back(Value::vreg(0));
        ops.push_back(p.second);
        (void)p.first;
    }
    return build_instruction(Opcode::Phi, ty, std::move(ops));
}

// Function implementation
inline Block* Function::create_block(std::string name) {
    blocks_.emplace_back(std::make_unique<Block>(this, std::move(name)));
    return blocks_.back().get();
}

inline Instruction* Function::make_instruction(Opcode op, Type ty, std::vector<Value> operands) {
    uint32_t id = next_inst_id_++;
    instructions_.emplace_back(id, op, ty, std::move(operands));
    return &instructions_.back();
}

inline Instruction* Function::make_constant(Constant c, Type ty) {
    uint32_t pool_idx = static_cast<uint32_t>(constants_.size());
    constants_.push_back(std::move(c));
    const_types_.push_back(ty);
    uint32_t id = next_inst_id_++;
    instructions_.emplace_back(id, Opcode::Copy, ty,
        std::vector<Value>{ Value::const_idx(pool_idx) });
    return &instructions_.back();
}

inline Value Function::const_i32(int32_t v) {
    return Value::vreg(make_constant(Constant(static_cast<int32_t>(v)), Type::i32())->id());
}
inline Value Function::const_i64(int64_t v) {
    return Value::vreg(make_constant(Constant(static_cast<int64_t>(v)), Type::i64())->id());
}
inline Value Function::const_bool(bool v) {
    return Value::vreg(make_constant(Constant(v), Type::i1())->id());
}
inline Value Function::const_ptr(std::nullptr_t) {
    return Value::vreg(make_constant(Constant(nullptr), Type::ptr())->id());
}

inline Instruction* Function::instruction_by_id(uint32_t id) {
    for (auto& inst : instructions_) {
        if (inst.id() == id) return &inst;
    }
    return nullptr;
}

inline Type Function::value_type(Value v) const {
    if (!v.valid()) return Type::void_ty();
    if (v.is_const()) return const_type(v.const_index());
    uint32_t index = v.vreg_index();
    if (index < param_types_.size()) return param_types_[index];
    if (Instruction* inst = const_cast<Function*>(this)->instruction_by_id(index)) return inst->type();
    return Type::void_ty();
}

inline std::string Function::to_string() const {
    std::ostringstream oss;
    oss << "func " << ret_type_.to_string() << " @" << name_ << "(";
    for (std::size_t i = 0; i < param_types_.size(); ++i) {
        if (i) oss << ", ";
        oss << param_types_[i].to_string() << " %p" << i;
    }
    oss << ") {\n";
    for (std::size_t i = 0; i < constants_.size(); ++i) {
        oss << "const $c" << i << " = " << constant_to_string(constants_[i], const_types_[i])
            << " : " << const_types_[i].to_string() << "\n";
    }
    for (auto const& b : blocks_) {
        oss << b->name() << ":\n";
        for (auto* inst : b->instructions()) {
            oss << inst->to_string() << "\n";
        }
    }
    oss << "}\n";
    return oss.str();
}

inline std::string Module::to_string() const {
    std::ostringstream oss;
    oss << "module \"" << name_ << "\"\n";
    for (auto const& f : functions_) {
        oss << f->to_string();
    }
    return oss.str();
}

} // namespace jiterati

#endif // JITERATI_HPP_INCLUDED
