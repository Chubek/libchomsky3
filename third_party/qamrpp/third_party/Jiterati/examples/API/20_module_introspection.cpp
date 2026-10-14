#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_module_introspection_details");
    auto* function = module.create_function<int(int, int)>("accumulate");
    auto* entry = function->create_block("entry");
    auto sum = entry->add(entry->arg(0), entry->arg(1));
    auto biased = entry->add(sum, function->const_i32(3));
    entry->ret(biased);

    std::cout << module.name() << '\n';
    std::cout << function->name() << ' ' << function->param_count() << ' ' << function->const_count() << '\n';
    std::cout << function->entry_block()->name() << ' ' << function->blocks().size() << '\n';
    std::cout << jiterati::opcode_name(function->instruction_by_id(biased.vreg_index())->opcode()) << '\n';
    std::cout << function->to_string();
    return function->value_type(biased) == jiterati::Type::i32() ? 0 : 1;
}
