#include "Jiterati-BE.hpp"

#include <iostream>

int main() {
    for (auto opcode : jiterati::lower_divrem_sequence(jiterati::Opcode::SRem, jiterati::Type::i32())) {
        std::cout << jiterati::opcode_name(opcode) << '\n';
    }
    for (auto opcode : jiterati::lower_divrem_sequence(jiterati::Opcode::Add, jiterati::Type::i64())) {
        std::cout << jiterati::opcode_name(opcode) << '\n';
    }
    return 0;
}
