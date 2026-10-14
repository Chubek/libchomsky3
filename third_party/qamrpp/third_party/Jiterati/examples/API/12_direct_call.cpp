#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_direct_call_details");
    auto* square = module.create_function<int(int)>("square");
    auto* square_entry = square->create_block("entry");
    square_entry->ret(square_entry->mul(square_entry->arg(0), square_entry->arg(0)));

    auto* affine = module.create_function<int(int)>("affine");
    auto* affine_entry = affine->create_block("entry");
    affine_entry->ret(affine_entry->add(affine_entry->mul(affine_entry->arg(0), affine->const_i32(3)), affine->const_i32(1)));

    auto* caller = module.create_function<int(int)>("square_affine_plus_one");
    auto* caller_entry = caller->create_block("entry");
    auto squared = caller_entry->call(square, {caller_entry->arg(0)});
    auto transformed = caller_entry->call(affine, {squared});
    caller_entry->ret(caller_entry->add(transformed, caller->const_i32(1)));

    std::cout << module.to_string();
    return 0;
}
