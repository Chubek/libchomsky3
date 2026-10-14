#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_min_select_details");
    auto* function = module.create_function<int(int, int, int)>("median_lower_bound_i32");
    auto* entry = function->create_block("entry");

    auto a = entry->arg(0);
    auto b = entry->arg(1);
    auto floor = entry->arg(2);
    auto min_ab = entry->select(entry->icmp_sle(a, b), a, b);
    auto max_floor = entry->select(entry->icmp_slt(min_ab, floor), floor, min_ab);
    entry->ret(max_floor);

    std::cout << module.to_string();
    return 0;
}
