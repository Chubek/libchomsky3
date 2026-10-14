#include "Jiterati.hpp"

#include <cstdint>
#include <iostream>

int main() {
    jiterati::Module module("api_pointer_roundtrip_details");
    auto* function = module.create_function<std::int64_t(void*)>("ptr_tag_bits");
    auto* entry = function->create_block("entry");

    auto bits = entry->ptrtoint(jiterati::Type::i64(), entry->arg(0));
    auto tagged = entry->bitwise_or(bits, function->const_i64(1));
    auto restored = entry->inttoptr(tagged);
    auto roundtrip = entry->ptrtoint(jiterati::Type::i64(), restored);
    entry->ret(entry->bitwise_and(roundtrip, function->const_i64(-8)));

    std::cout << module.to_string();
    return 0;
}
