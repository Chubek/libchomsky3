#ifndef QAMRPP_PRINT_AST_HPP
#define QAMRPP_PRINT_AST_HPP

#include <iostream>

#include "../QaMRpp.hpp"

namespace qamrpp {

class PrintASTPlugin : public Plugin {
public:
    const char* name() const { return "PrintAST"; }

    void install(Context& ctx) {
        ctx.add_hook(HookPoint::AfterParse, [](Context&, const HookPayload& payload) {
            if (!payload.ast) return;
            std::cout << "[PrintAST] parsed root node type=" << payload.ast->type << "\n";
        });
    }
};

} // namespace qamrpp

#endif
