#include <iostream>

#include "../QaMRpp.hpp"

int main() {
    qamrpp::Context ctx;
    ctx.load_standard_library(qamrpp::StdLib::CORE | qamrpp::StdLib::MATH);

    qamrpp::ValuePtr out = ctx.run("type(42)");
    std::cout << out->to_string() << "\n";
    return 0;
}
