#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_alloca_load_store_details");
    auto* function = module.create_function<int(int, int)>("two_slot_reload_sum");
    auto* entry = function->create_block("entry");

    auto lhs_slot = entry->alloca(jiterati::Type::i32());
    auto rhs_slot = entry->alloca(jiterati::Type::i32());
    entry->store(lhs_slot, entry->arg(0));
    entry->store(rhs_slot, entry->arg(1));
    auto lhs = entry->load(jiterati::Type::i32(), lhs_slot);
    auto rhs = entry->load(jiterati::Type::i32(), rhs_slot);
    entry->ret(entry->add(entry->mul(lhs, function->const_i32(2)), rhs));

    std::cout << module.to_string();
    return 0;
}
