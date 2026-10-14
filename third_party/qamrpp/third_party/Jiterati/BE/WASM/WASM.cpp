#include "WASM.hpp"

namespace jiterati::be::wasm {

ValueClass value_class(ir::Type type) {
    switch (type.scalar) {
        case ir::ScalarType::Bool:
        case ir::ScalarType::I32: return ValueClass::I32;
        case ir::ScalarType::I64:
        case ir::ScalarType::Ptr: return ValueClass::I64;
        case ir::ScalarType::F32: return ValueClass::F32;
        case ir::ScalarType::F64: return ValueClass::F64;
        case ir::ScalarType::Void: return ValueClass::Void;
    }
    return ValueClass::I32;
}

char const* value_class_name(ValueClass cls) {
    switch (cls) {
        case ValueClass::I32:  return "i32";
        case ValueClass::I64:  return "i64";
        case ValueClass::F32:  return "f32";
        case ValueClass::F64:  return "f64";
        case ValueClass::Void: return "void";
    }
    return "i32";
}

} // namespace jiterati::be::wasm
