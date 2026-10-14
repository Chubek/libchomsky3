#include "../../IR/ICompiler.hpp"
#include "Emit.hpp"
#include "ISel.hpp"
#include "Peephole.hpp"
#include "RegAlloc.hpp"
#include "Rewrite.hpp"

#include <vector>

namespace jiterati::ir {
namespace {

class WASMIRCompiler final : public ICompiler {
public:
    std::string target() const override { return "wasm"; }
    ArtifactFormat format() const override { return ArtifactFormat::IRText; }
    CompilationArtifact compile(TACModule const& module, CompileOptions options) override {
        TACModule work = module;
        if (options.optimize_tac) peephole(work);
        CompilationArtifact artifact{ target(), format() };
        if (options.verify_input) artifact.diagnostics = validate(work);
        if (!artifact.ok()) return artifact;
        std::vector<be::wasm::WasmFunction> functions;
        for (auto const& function : work.functions) {
            auto wasm = be::wasm::select(function);
            be::wasm::allocate_registers(wasm);
            be::wasm::rewrite(wasm);
            be::wasm::peephole(wasm);
            functions.push_back(std::move(wasm));
        }
        artifact.text = be::wasm::emit_module(functions);
        artifact.bytes.assign(artifact.text.begin(), artifact.text.end());
        return artifact;
    }
};

} // namespace
std::unique_ptr<ICompiler> create_wasm_ir_compiler() { return std::make_unique<WASMIRCompiler>(); }
} // namespace jiterati::ir
