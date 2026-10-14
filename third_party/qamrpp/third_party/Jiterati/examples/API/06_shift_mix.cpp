#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_shift_mix_details");
    auto* function = module.create_function<int(int, int)>("rotate_like_mix");
    auto* entry = function->create_block("entry");

    auto x = entry->arg(0);
    auto amount = entry->bitwise_and(entry->arg(1), function->const_i32(31));
    auto left = entry->shl(x, amount);
    auto right = entry->lshr(x, function->const_i32(3));
    auto sign = entry->ashr(x, function->const_i32(31));
    entry->ret(entry->bitwise_xor(entry->bitwise_or(left, right), sign));

    std::cout << module.to_string();
    return 0;
}
