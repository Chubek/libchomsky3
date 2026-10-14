#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_compare_chain_details");
    auto* function = module.create_function<bool(int, int, int)>("strictly_between_i32");
    auto* entry = function->create_block("entry");

    auto lo = entry->arg(0);
    auto value = entry->arg(1);
    auto hi = entry->arg(2);
    auto above_lo = entry->icmp_slt(lo, value);
    auto below_hi = entry->icmp_slt(value, hi);
    auto ordered = entry->icmp_slt(lo, hi);
    entry->ret(entry->bitwise_and(entry->bitwise_and(above_lo, below_hi), ordered));

    std::cout << module.to_string();
    return 0;
}
