#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_abs_select_details");
    auto* function = module.create_function<int(int)>("abs_or_zero_i32");
    auto* entry = function->create_block("entry");

    auto x = entry->arg(0);
    auto zero = function->const_i32(0);
    auto negated = entry->sub(zero, x);
    auto is_negative = entry->icmp_slt(x, zero);
    auto absolute = entry->select(is_negative, negated, x);
    auto is_zero = entry->icmp_eq(x, zero);
    entry->ret(entry->select(is_zero, zero, absolute));

    std::cout << module.to_string();
    return 0;
}
