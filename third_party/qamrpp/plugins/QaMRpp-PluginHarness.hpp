#ifndef QAMRPP_PLUGIN_HARNESS_HPP
#define QAMRPP_PLUGIN_HARNESS_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QaMRpp.hpp>

/* ============================================================
 * Plugin includes — one per subdirectory
 * ============================================================ */

#include "compile-to-c/QaMRpp-Compile2C.hpp"
#include "compile-to-c/QaMRpp-CCBackend.hpp"
#include "compile-to-c/c_translation.hpp"

#include "compile-to-wasm/QaMRpp-Compile2WASM.hpp"
#include "compile-to-wasm/wasm_translation.hpp"

#include "compile-to-bytecode/QaMRpp-Compile2Bytecode.hpp"
#include "compile-to-bytecode/bytecode_translation.hpp"

#include "qbf/QaMRpp-QBF.hpp"
#include "dump/QaMRpp-Dump.hpp"
#include "readline/QaMRpp-Readline.hpp"
#include "serialize2json/QaMRpp-Serialize2JSON.hpp"
#include "print-ast/QaMRpp-PrintAST.hpp"

#include "jit-backend/QaMRpp-JITBackend.hpp"
#include "aot-backend/QaMRpp-AOTBackend.hpp"

/* ============================================================
 * Harness — single-point plugin enablement
 * ============================================================ */

namespace qamrpp {
namespace harness {

/* Returns a new instance of every known Plugin. */
inline std::vector<std::unique_ptr<Plugin>> all_plugins() {
    std::vector<std::unique_ptr<Plugin>> out;
    out.push_back(std::make_unique<Compile2CPlugin>());
    out.push_back(std::make_unique<CTranslationPlugin>());
    out.push_back(std::make_unique<CCBackendPlugin>());
    out.push_back(std::make_unique<Compile2WASMPlugin>());
    out.push_back(std::make_unique<WASMTranslationPlugin>());
    out.push_back(std::make_unique<Compile2BytecodePlugin>());
    out.push_back(std::make_unique<BytecodeTranslationPlugin>());
    out.push_back(std::make_unique<PrintASTPlugin>());
    out.push_back(std::make_unique<Serialize2JSONPlugin>());
    out.push_back(std::make_unique<DumpPlugin>());
    out.push_back(std::make_unique<JITBackendPlugin>());
    out.push_back(std::make_unique<AOTBackendPlugin>());
    return out;
}

/* Install every known plugin into the given context. */
inline void install_all(Context& ctx) {
    auto plugins = all_plugins();
    for (auto& p : plugins) {
        p->install(ctx);
    }
}

/* Install a single plugin by name.  Returns true if installed. */
inline bool install_one(Context& ctx, const std::string& name) {
    auto plugins = all_plugins();
    for (auto& p : plugins) {
        if (p->name() == name) {
            p->install(ctx);
            return true;
        }
    }
    return false;
}

/* Install a list of plugins by name.  Returns the count of those installed. */
inline size_t install_many(Context& ctx, const std::vector<std::string>& names) {
    size_t count = 0;
    for (const auto& n : names) {
        if (install_one(ctx, n)) ++count;
    }
    return count;
}

} // namespace harness
} // namespace qamrpp

#endif /* QAMRPP_PLUGIN_HARNESS_HPP */
