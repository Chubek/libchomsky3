#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_bitwise_mask_details");
    auto* function = module.create_function<int(int)>("pack_low_nibbles");
    auto* entry = function->create_block("entry");

    auto x = entry->arg(0);
    auto low = entry->bitwise_and(x, function->const_i32(0x0f));
    auto high = entry->bitwise_and(entry->lshr(x, function->const_i32(4)), function->const_i32(0x0f));
    auto packed = entry->bitwise_or(entry->shl(high, function->const_i32(8)), low);
    entry->ret(packed);

    std::cout << module.to_string();
    return 0;
}
