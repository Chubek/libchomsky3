#include "Jiterati.hpp"

#include <cstdint>
#include <iostream>

int main() {
    jiterati::Module module("api_mul_i64_details");
    auto* function = module.create_function<std::int64_t(std::int64_t, std::int64_t)>("mul_accumulate_i64");
    auto* entry = function->create_block("entry");

    auto x = entry->arg(0);
    auto y = entry->arg(1);
    auto product = entry->mul(x, y);
    auto doubled = entry->add(product, product);
    auto adjusted = entry->sub(doubled, function->const_i64(17));
    entry->ret(adjusted);

    std::cout << module.to_string();
    return 0;
}
