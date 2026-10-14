#include "Jiterati-Pass.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_pass_pipeline_details");
    auto* folded = module.create_function<int(int)>("fold_candidate");
    auto* folded_entry = folded->create_block("entry");
    auto constant = folded_entry->mul(folded->const_i32(6), folded->const_i32(7));
    folded_entry->ret(folded_entry->add(folded_entry->arg(0), constant));

    auto* branchy = module.create_function<int(int)>("branch_candidate");
    auto* entry = branchy->create_block("entry");
    auto* negative = branchy->create_block("negative");
    auto* non_negative = branchy->create_block("non_negative");
    entry->cond_br(entry->icmp_slt(entry->arg(0), branchy->const_i32(0)), negative, non_negative);
    negative->ret(branchy->const_i32(-1));
    non_negative->ret(branchy->const_i32(1));

    auto pipeline = jiterati::parse_jpl_pipeline("cfg; constant-folding; strength-reduction; dce");
    pipeline.run(module);
    std::cout << module.to_string();
    return 0;
}
