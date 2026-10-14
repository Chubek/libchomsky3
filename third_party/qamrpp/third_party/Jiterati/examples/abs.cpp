#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("abs_example");
    jiterati::Function* function = module.create_function<int(int)>("abs_i32");
    jiterati::Block* entry = function->create_block("entry");
    jiterati::Value x = entry->arg(0);
    jiterati::Value zero = function->const_i32(0);
    jiterati::Value negative = entry->sub(zero, x);
    jiterati::Value is_negative = entry->icmp_slt(x, zero);
    jiterati::Value result = entry->select(is_negative, negative, x);
    entry->ret(result);

    std::cout << module.to_string();
    return 0;
}
