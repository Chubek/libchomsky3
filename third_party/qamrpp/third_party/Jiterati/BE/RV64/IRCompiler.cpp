#include "../../IR/ICompiler.hpp"
#include "Emit.hpp"
#include "ISel.hpp"
#include "Peephole.hpp"
#include "RegAlloc.hpp"
#include "Rewrite.hpp"

#include <sstream>

namespace jiterati::ir {
namespace {

class RV64IRCompiler final : public ICompiler {
public:
    std::string target() const override { return "rv64"; }
    ArtifactFormat format() const override { return ArtifactFormat::Assembly; }
    CompilationArtifact compile(TACModule const& module, CompileOptions options) override {
        TACModule work = module;
        if (options.optimize_tac) peephole(work);
        CompilationArtifact artifact{ target(), format() };
        if (options.verify_input) artifact.diagnostics = validate(work);
        if (!artifact.ok()) return artifact;
        std::ostringstream text;
        for (auto const& function : work.functions) {
            auto machine = be::rv64::select(function);
            be::rv64::allocate_registers(machine);
            be::rv64::rewrite(machine);
            be::rv64::peephole(machine);
            text << be::rv64::emit_assembly(machine) << '\n';
        }
        artifact.text = text.str(); artifact.bytes.assign(artifact.text.begin(), artifact.text.end()); return artifact;
    }
};

} // namespace
std::unique_ptr<ICompiler> create_rv64_ir_compiler() { return std::make_unique<RV64IRCompiler>(); }
} // namespace jiterati::ir
