#include <iostream>

#include "../QaMRpp.hpp"

int main() {
    qamrpp::Context ctx;
    qamrpp::ValuePtr out = ctx.run("1 + 2");
    std::cout << out->to_string() << "\n";
    return 0;
}
