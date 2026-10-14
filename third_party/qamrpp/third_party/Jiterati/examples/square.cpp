#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("square_example");
    jiterati::Function* function = module.create_function<int(int)>("square");
    jiterati::Block* entry = function->create_block("entry");
    jiterati::Value x = entry->arg(0);
    jiterati::Value result = entry->mul(x, x);
    entry->ret(result);

    std::cout << module.to_string();
    return 0;
}
