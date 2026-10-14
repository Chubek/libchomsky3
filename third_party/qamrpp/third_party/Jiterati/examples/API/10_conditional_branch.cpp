#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_conditional_branch_details");
    auto* function = module.create_function<int(int)>("signum_branch_with_zero");
    auto* entry = function->create_block("entry");
    auto* negative = function->create_block("negative");
    auto* non_negative = function->create_block("non_negative");
    auto* zero_block = function->create_block("zero");
    auto* positive = function->create_block("positive");

    auto x = entry->arg(0);
    auto zero = function->const_i32(0);
    entry->cond_br(entry->icmp_slt(x, zero), negative, non_negative);
    negative->ret(function->const_i32(-1));
    non_negative->cond_br(non_negative->icmp_eq(x, zero), zero_block, positive);
    zero_block->ret(zero);
    positive->ret(function->const_i32(1));

    std::cout << module.to_string();
    return 0;
}
