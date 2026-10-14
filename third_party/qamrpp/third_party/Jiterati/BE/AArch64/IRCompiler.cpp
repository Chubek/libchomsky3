#include "../../IR/ICompiler.hpp"
#include "Emit.hpp"
#include "ISel.hpp"
#include "Peephole.hpp"
#include "RegAlloc.hpp"
#include "Rewrite.hpp"

#include <sstream>

namespace jiterati::ir {
namespace {

class AArch64IRCompiler final : public ICompiler {
public:
    std::string target() const override { return "aarch64"; }
    ArtifactFormat format() const override { return ArtifactFormat::Assembly; }
    CompilationArtifact compile(TACModule const& module, CompileOptions options) override {
        TACModule work = module;
        if (options.optimize_tac) peephole(work);
        CompilationArtifact artifact{ target(), format() };
        if (options.verify_input) artifact.diagnostics = validate(work);
        if (!artifact.ok()) return artifact;
        std::ostringstream text;
        for (auto const& function : work.functions) {
            auto machine = be::aarch64::select(function);
            be::aarch64::allocate_registers(machine);
            be::aarch64::rewrite(machine);
            be::aarch64::peephole(machine);
            text << be::aarch64::emit_assembly(machine) << '\n';
        }
        artifact.text = text.str(); artifact.bytes.assign(artifact.text.begin(), artifact.text.end()); return artifact;
    }
};

} // namespace
std::unique_ptr<ICompiler> create_aarch64_ir_compiler() { return std::make_unique<AArch64IRCompiler>(); }
} // namespace jiterati::ir
