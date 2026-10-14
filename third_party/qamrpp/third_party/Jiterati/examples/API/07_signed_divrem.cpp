#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_signed_divrem_details");
    auto* function = module.create_function<int(int, int)>("signed_divrem_checksum");
    auto* entry = function->create_block("entry");

    auto numerator = entry->arg(0);
    auto denominator = entry->arg(1);
    auto quotient = entry->sdiv(numerator, denominator);
    auto remainder = entry->srem(numerator, denominator);
    auto recomposed = entry->add(entry->mul(quotient, denominator), remainder);
    entry->ret(entry->sub(recomposed, numerator));

    std::cout << module.to_string();
    return 0;
}
