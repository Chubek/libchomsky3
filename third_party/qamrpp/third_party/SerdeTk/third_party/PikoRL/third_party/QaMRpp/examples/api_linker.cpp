#include <iostream>

#include "../QaMRpp.hpp"

int main() {
    qamrpp::Context ctx;
    qamrpp::Linker linker;
    linker.add_source("first.lua", "x = 7");
    linker.add_source("second.lua", "x + 5");
    qamrpp::ValuePtr out = linker.link(ctx);
    std::cout << out->to_string() << "\n";
    return 0;
}
