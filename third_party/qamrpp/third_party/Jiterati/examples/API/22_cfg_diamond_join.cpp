#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_cfg_diamond_join");
    auto* function = module.create_function<int(int, int)>("abs_delta_diamond");
    auto* entry = function->create_block("entry");
    auto* lhs_ge_rhs = function->create_block("lhs_ge_rhs");
    auto* rhs_gt_lhs = function->create_block("rhs_gt_lhs");
    auto* join = function->create_block("join");

    auto lhs = entry->arg(0);
    auto rhs = entry->arg(1);
    auto zero = function->const_i32(0);
    entry->cond_br(entry->icmp_sge(lhs, rhs), lhs_ge_rhs, rhs_gt_lhs);
    auto forward = lhs_ge_rhs->sub(lhs, rhs);
    lhs_ge_rhs->br(join);
    auto reverse = rhs_gt_lhs->sub(rhs, lhs);
    rhs_gt_lhs->br(join);
    auto result = join->phi(jiterati::Type::i32(), {{lhs_ge_rhs, forward}, {rhs_gt_lhs, reverse}});
    join->ret(join->add(result, zero));

    std::cout << module.to_string();
    return 0;
}
