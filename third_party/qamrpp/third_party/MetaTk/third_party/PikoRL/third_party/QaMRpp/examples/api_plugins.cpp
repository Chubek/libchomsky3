#include <iostream>

#include "../QaMRpp.hpp"
#include "../plugins/QaMRpp-PrintAST.hpp"

int main() {
    qamrpp::Context ctx;
    qamrpp::PrintASTPlugin plugin;
    plugin.install(ctx);

    qamrpp::ValuePtr out = ctx.run("x = 3; x + 4");
    std::cout << out->to_string() << "\n";
    return 0;
}
