#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_add_i32_details");
    auto* function = module.create_function<int(int, int)>("add_i32_bias");
    auto* entry = function->create_block("entry");

    auto lhs = entry->arg(0);
    auto rhs = entry->arg(1);
    auto sum = entry->add(lhs, rhs);
    auto biased = entry->add(sum, function->const_i32(12));
    auto normalized = entry->sub(biased, function->const_i32(5));
    entry->ret(normalized);

    std::cout << module.to_string();
    return 0;
}
