#ifndef JITERATI_BE_WASM_WASM_HPP_INCLUDED
#define JITERATI_BE_WASM_WASM_HPP_INCLUDED

#include "../../IR/IR.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace jiterati::be::wasm {

// WASM is a structured stack machine: there are no physical registers, so the
// "allocation" phase assigns each value to a local instead.  Values carry a
// value class rather than a register class.
enum class ValueClass { I32, I64, F32, F64, Void };

struct Local {
    std::string name;
    ValueClass cls = ValueClass::I32;
};

struct WasmInst {
    std::string opcode;
    std::vector<std::string> operands;
    std::string comment;
};

struct WasmBlock {
    std::string label;
    std::vector<WasmInst> instructions;
};

struct WasmFunction {
    std::string name;
    ir::Type return_type = ir::Type::void_ty();
    std::vector<ir::Parameter> parameters;
    std::vector<Local> locals;
    std::vector<WasmBlock> blocks;
    std::map<std::string, std::size_t> local_index;
};

ValueClass value_class(ir::Type type);
char const* value_class_name(ValueClass cls);

WasmFunction select(ir::TACFunction const& function);
void allocate_registers(WasmFunction& function);
void rewrite(WasmFunction& function);
bool peephole(WasmFunction& function);
std::string emit_module(std::vector<WasmFunction> const& functions);
std::string emit_assembly(WasmFunction const& function);

} // namespace jiterati::be::wasm

#endif // JITERATI_BE_WASM_WASM_HPP_INCLUDED
