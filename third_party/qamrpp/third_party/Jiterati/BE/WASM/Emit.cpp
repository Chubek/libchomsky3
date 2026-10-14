#include "Emit.hpp"

#include <sstream>

namespace jiterati::be::wasm {
namespace {

// Render an operand as a WASM local reference: %name -> $name, everything else
// (literals, block labels) is emitted verbatim.
std::string local(std::string const& op) {
    if (!op.empty() && op[0] == '%') return "$" + op.substr(1);
    return op;
}

} // namespace

std::string emit_assembly(WasmFunction const& function) {
    std::ostringstream out;
    out << "  (func $" << function.name;
    for (auto const& p : function.parameters) {
        out << " (param $" << p.name << ' ' << value_class_name(value_class(p.type)) << ')';
    }
    if (!function.return_type.is_void()) {
        out << " (result " << value_class_name(value_class(function.return_type)) << ')';
    }
    out << "\n";

    for (auto const& l : function.locals) {
        if (l.name.rfind("%t", 0) == 0) {
            out << "    (local " << local(l.name) << ' ' << value_class_name(l.cls) << ")\n";
        }
    }

    for (auto const& block : function.blocks) {
        out << "    ;; " << block.label << "\n";
        for (auto const& inst : block.instructions) {
            if (!inst.comment.empty()) out << "    ;; " << inst.comment << "\n";
            if (inst.opcode == "nop") {
                out << "    nop\n";
                continue;
            }
            out << "    (" << inst.opcode;
            for (auto const& operand : inst.operands) out << ' ' << local(operand);
            out << ")\n";
        }
    }
    out << "  )\n";
    return out.str();
}

std::string emit_module(std::vector<WasmFunction> const& functions) {
    std::ostringstream out;
    out << "(module ;; Jiterati WASM text backend (illustrative; not stack-correct WAT)\n";
    for (auto const& function : functions) out << emit_assembly(function);
    out << ")\n";
    return out.str();
}

} // namespace jiterati::be::wasm
