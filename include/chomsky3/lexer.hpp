#ifndef CHOMSKY3_LEXER_HPP
#define CHOMSKY3_LEXER_HPP

#include <array>
#include <string>
#include <vector>

namespace chomsky3 {
// Byte-oriented lexer DFA. -1 denotes a dead transition/nonaccepting state.
struct lexer_dfa {
    std::vector<std::array<int, 256>> transitions;
    std::vector<int> accepts;
};
lexer_dfa compile_lexer(const std::vector<std::string>& patterns,
                        std::size_t max_states = 4096);
}
#endif
