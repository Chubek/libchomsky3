#include "IR.hpp"

#include <set>
#include <sstream>

namespace jiterati::ir {

std::string diagnostic_str(Diagnostic const& diagnostic) {
    std::ostringstream out;
    switch (diagnostic.severity) {
        case Diagnostic::Severity::Note: out << "note"; break;
        case Diagnostic::Severity::Warning: out << "warning"; break;
        case Diagnostic::Severity::Error: out << "error"; break;
    }
    if (!diagnostic.function.empty()) out << " in " << diagnostic.function;
    out << " bb" << diagnostic.block << ": " << diagnostic.message;
    return out.str();
}

std::vector<Diagnostic> validate(TACFunction const& function) {
    std::vector<Diagnostic> diagnostics;
    std::set<BlockId> block_ids;
    std::set<TempId> definitions;

    auto report = [&](std::string message, BlockId block) {
        diagnostics.push_back({ Diagnostic::Severity::Error, std::move(message), function.name, block });
    };

    for (auto const& block : function.blocks) {
        if (!block_ids.insert(block.id).second) report("duplicate block id", block.id);
    }

    for (auto const& block : function.blocks) {
        if (!block.has_terminator()) report("block has no terminator", block.id);
        for (auto const& instruction : block.instructions) {
            if (instruction.result.has_value()) {
                TACValue const& result = *instruction.result;
                if (result.kind != TACValue::Kind::Temporary) report("instruction result is not a temporary", block.id);
                if (!definitions.insert(result.index).second) report("temporary defined more than once", block.id);
            }
            for (auto target : instruction.targets) {
                if (block_ids.count(target) == 0) report("branch targets missing block", block.id);
            }
            for (auto const& operand : instruction.operands) {
                if (operand.kind == TACValue::Kind::Temporary && definitions.count(operand.index) == 0) {
                    report("temporary used before definition: " + operand.str(), block.id);
                }
                if (operand.kind == TACValue::Kind::Parameter && operand.index >= function.parameters.size()) {
                    report("parameter index out of range: " + operand.str(), block.id);
                }
            }
        }
    }

    return diagnostics;
}

std::vector<Diagnostic> validate(TACModule const& module) {
    std::vector<Diagnostic> diagnostics;
    for (auto const& function : module.functions) {
        auto function_diagnostics = validate(function);
        diagnostics.insert(diagnostics.end(), function_diagnostics.begin(), function_diagnostics.end());
    }
    return diagnostics;
}

} // namespace jiterati::ir
