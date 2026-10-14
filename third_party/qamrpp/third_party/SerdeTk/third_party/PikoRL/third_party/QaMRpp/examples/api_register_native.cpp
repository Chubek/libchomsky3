#include <iostream>
#include <vector>

#include "../QaMRpp.hpp"

int main() {
    qamrpp::Context ctx;
    ctx.register_native("add", [](qamrpp::Context&, std::vector<qamrpp::ValuePtr>& args) -> qamrpp::ValuePtr {
        int64_t a = args.size() > 0 ? args[0]->int_value : 0;
        int64_t b = args.size() > 1 ? args[1]->int_value : 0;
        return std::make_shared<qamrpp::Value>(a + b);
    });

    qamrpp::ValuePtr out = ctx.run("add(20, 22)");
    std::cout << out->to_string() << "\n";
    return 0;
}
