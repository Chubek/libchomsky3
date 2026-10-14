#ifndef JITERATI_IR_ICOMPILER_HPP_INCLUDED
#define JITERATI_IR_ICOMPILER_HPP_INCLUDED

#include "IR.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace jiterati::ir {

enum class ArtifactFormat {
    IRText,
    Assembly,
    Object,
    WasmBinary,
};

struct CompileOptions {
    bool optimize_tac = true;
    bool verify_input = true;
    bool emit_comments = true;
};

struct CompilationArtifact {
    std::string target;
    ArtifactFormat format = ArtifactFormat::IRText;
    std::vector<std::uint8_t> bytes;
    std::string text;
    std::vector<Diagnostic> diagnostics;

    bool ok() const;
};

class ICompiler {
public:
    virtual ~ICompiler() = default;

    virtual std::string target() const = 0;
    virtual ArtifactFormat format() const = 0;
    virtual CompilationArtifact compile(TACModule const& module, CompileOptions options = {}) = 0;

    CompilationArtifact compile(TerseModule const& module, CompileOptions options = {});
};

inline bool CompilationArtifact::ok() const {
    for (auto const& diagnostic : diagnostics) {
        if (diagnostic.severity == Diagnostic::Severity::Error) return false;
    }
    return true;
}

inline CompilationArtifact ICompiler::compile(TerseModule const& module, CompileOptions options) {
    TACModule lowered = lower_to_tac(module);
    return compile(lowered, options);
}

std::unique_ptr<ICompiler> create_amd64_ir_compiler();
std::unique_ptr<ICompiler> create_aarch64_ir_compiler();
std::unique_ptr<ICompiler> create_rv64_ir_compiler();
std::unique_ptr<ICompiler> create_wasm_ir_compiler();

} // namespace jiterati::ir

#endif // JITERATI_IR_ICOMPILER_HPP_INCLUDED
