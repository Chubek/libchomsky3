#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_void_return_details");
    auto* noop = module.create_function<void()>("do_nothing");
    auto* noop_entry = noop->create_block("entry");
    noop_entry->ret_void();

    auto* conditional = module.create_function<void(bool)>("branch_then_return");
    auto* entry = conditional->create_block("entry");
    auto* fast = conditional->create_block("fast");
    auto* slow = conditional->create_block("slow");
    entry->cond_br(entry->arg(0), fast, slow);
    fast->ret_void();
    slow->ret_void();

    std::cout << module.to_string();
    return 0;
}
