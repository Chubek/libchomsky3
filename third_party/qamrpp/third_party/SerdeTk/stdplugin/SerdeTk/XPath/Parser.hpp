#pragma once

#include "CodeTree.hpp"
#include "Tokenizer.hpp"

#include <string_view>
#include <vector>

namespace serdetk::xpath {

class Parser {
public:
    [[nodiscard]] CodeTreePtr parse(std::string_view expression) const;

private:
    class State {
    public:
        explicit State(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}
        CodeTreePtr expression();
        [[nodiscard]] bool at_end() const;

    private:
        const Token& current() const;
        bool accept(TokenKind kind);
        const Token& expect(TokenKind kind, std::string_view message);
        CodeTreePtr binary(unsigned precedence);
        CodeTreePtr unary();
        CodeTreePtr primary();
        CodeTreePtr path(bool absolute);
        void predicates(PathStep& step);
        unsigned precedence(TokenKind kind) const;

        std::vector<Token> tokens_;
        std::size_t position_ {0};
    };
};

} // namespace serdetk::xpath
