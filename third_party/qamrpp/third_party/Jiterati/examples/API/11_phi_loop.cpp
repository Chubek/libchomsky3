#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_phi_loop_details");
    auto* function = module.create_function<int(int)>("sum_to_n_shape");
    auto* entry = function->create_block("entry");
    auto* loop = function->create_block("loop");
    auto* exit = function->create_block("exit");

    auto n = entry->arg(0);
    entry->br(loop);
    auto index = loop->phi(jiterati::Type::i32(), {{entry, function->const_i32(0)}, {loop, function->const_i32(1)}});
    auto acc = loop->phi(jiterati::Type::i32(), {{entry, function->const_i32(0)}, {loop, index}});
    auto next = loop->add(index, function->const_i32(1));
    auto total = loop->add(acc, next);
    loop->cond_br(loop->icmp_slt(next, n), loop, exit);
    exit->ret(total);

    std::cout << module.to_string();
    return 0;
}
