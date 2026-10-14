#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_multi_function_module_details");
    auto* add = module.create_function<int(int, int)>("add");
    auto* add_entry = add->create_block("entry");
    add_entry->ret(add_entry->add(add_entry->arg(0), add_entry->arg(1)));

    auto* sub = module.create_function<int(int, int)>("sub");
    auto* sub_entry = sub->create_block("entry");
    sub_entry->ret(sub_entry->sub(sub_entry->arg(0), sub_entry->arg(1)));

    auto* fused = module.create_function<int(int, int)>("add_then_sub_rhs");
    auto* fused_entry = fused->create_block("entry");
    auto sum = fused_entry->call(add, {fused_entry->arg(0), fused_entry->arg(1)});
    auto delta = fused_entry->call(sub, {sum, fused_entry->arg(1)});
    fused_entry->ret(delta);

    std::cout << module.to_string();
    return module.find_function("add_then_sub_rhs") == nullptr ? 1 : 0;
}
