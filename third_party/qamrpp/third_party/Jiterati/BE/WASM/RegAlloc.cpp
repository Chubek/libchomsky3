#include "RegAlloc.hpp"

namespace jiterati::be::wasm {

// WASM has no registers: every value lives in a local.  Parameters occupy the
// first local slots; each temporary (%t*) encountered gets the next slot.
void allocate_registers(WasmFunction& function) {
    for (auto const& parameter : function.parameters) {
        std::string name = "%" + parameter.name;
        function.local_index[name] = function.locals.size();
        function.locals.push_back({ name, value_class(parameter.type) });
    }
    for (auto& block : function.blocks) {
        for (auto& inst : block.instructions) {
            for (auto const& op : inst.operands) {
                if (op.rfind("%t", 0) != 0 || function.local_index.count(op) != 0) continue;
                function.local_index[op] = function.locals.size();
                function.locals.push_back({ op, ValueClass::I64 });
            }
        }
    }
}

} // namespace jiterati::be::wasm
