#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_const_bool_select_details");
    auto* function = module.create_function<int(int)>("choose_constant_or_input");
    auto* entry = function->create_block("entry");

    auto input = entry->arg(0);
    auto seven = function->const_i32(7);
    auto nine = function->const_i32(9);
    auto constant_choice = entry->select(function->const_bool(true), seven, nine);
    auto use_input = entry->icmp_sgt(input, constant_choice);
    entry->ret(entry->select(use_input, input, constant_choice));

    std::cout << module.to_string();
    return 0;
}
