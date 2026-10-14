#include "../../IR/ICompiler.hpp"
#include "Emit.hpp"
#include "ISel.hpp"
#include "Peephole.hpp"
#include "RegAlloc.hpp"
#include "Rewrite.hpp"

#include <sstream>

namespace jiterati::ir {
namespace {

class AMD64IRCompiler final : public ICompiler {
public:
    std::string target() const override { return "amd64"; }
    ArtifactFormat format() const override { return ArtifactFormat::Assembly; }

    CompilationArtifact compile(TACModule const& module, CompileOptions options) override {
        TACModule work = module;
        if (options.optimize_tac) peephole(work);

        CompilationArtifact artifact;
        artifact.target = target();
        artifact.format = format();
        if (options.verify_input) artifact.diagnostics = validate(work);
        if (!artifact.ok()) return artifact;

        std::ostringstream text;
        for (auto const& function : work.functions) {
            auto machine = be::amd64::select(function);
            be::amd64::allocate_registers(machine);
            be::amd64::rewrite(machine);
            be::amd64::peephole(machine);
            text << be::amd64::emit_assembly(machine) << '\n';
        }
        artifact.text = text.str();
        artifact.bytes.assign(artifact.text.begin(), artifact.text.end());
        return artifact;
    }
};

} // namespace

std::unique_ptr<ICompiler> create_amd64_ir_compiler() {
    return std::make_unique<AMD64IRCompiler>();
}

} // namespace jiterati::ir
