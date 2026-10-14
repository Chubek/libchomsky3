#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_type_and_constant_pool");
    auto* function = module.create_function<int(bool, int)>("choose_and_bias");
    auto* entry = function->create_block("entry");

    auto selected = entry->select(entry->arg(0), function->const_i32(64), entry->arg(1));
    auto biased = entry->add(selected, function->const_i32(7));
    auto narrowed = entry->trunc(jiterati::Type::i8(), biased);
    auto widened = entry->sext(jiterati::Type::i32(), narrowed);
    entry->ret(widened);

    std::cout << function->param_count() << ' ' << function->const_count() << '\n';
    std::cout << module.to_string();
    return function->value_type(widened) == jiterati::Type::i32() ? 0 : 1;
}
