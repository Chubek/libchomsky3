#include "Jiterati.hpp"

#include <cstdint>
#include <iostream>

int main() {
    jiterati::Module module("api_unsigned_divrem_details");
    auto* function = module.create_function<std::uint32_t(std::uint32_t, std::uint32_t)>("unsigned_divrem_pack");
    auto* entry = function->create_block("entry");

    auto value = entry->arg(0);
    auto divisor = entry->arg(1);
    auto quotient = entry->udiv(value, divisor);
    auto remainder = entry->urem(value, divisor);
    auto packed = entry->bitwise_or(entry->shl(quotient, function->const_i32(16)), remainder);
    auto masked = entry->bitwise_and(packed, function->const_i32(0x00ffffff));
    entry->ret(masked);

    std::cout << module.to_string();
    return 0;
}
