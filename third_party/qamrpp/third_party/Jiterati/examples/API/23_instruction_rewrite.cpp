#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_instruction_rewrite");
    auto* function = module.create_function<int(int)>("rewrite_operand_demo");
    auto* entry = function->create_block("entry");

    auto original = entry->add(entry->arg(0), function->const_i32(1));
    auto replacement = function->const_i32(42);
    auto* instruction = function->instruction_by_id(original.vreg_index());
    instruction->set_operand(1, replacement);
    entry->ret(original);

    std::cout << instruction->to_string() << '\n';
    std::cout << module.to_string();
    return instruction->operand(1) == replacement ? 0 : 1;
}
