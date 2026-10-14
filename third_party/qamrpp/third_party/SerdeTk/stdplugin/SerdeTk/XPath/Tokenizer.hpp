#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace serdetk::xpath {

enum class TokenKind {
    End, Name, String, Number, Slash, DoubleSlash, Dot, DotDot, At, Star, Question,
    LParen, RParen, LBracket, RBracket, LBrace, RBrace, Comma, Colon, Plus, Minus,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual, Pipe, Assign,
    KeywordAnd, KeywordOr, KeywordDiv, KeywordIdiv, KeywordMod
};

struct Token {
    TokenKind kind {TokenKind::End};
    std::string lexeme {};
    std::size_t offset {0};
};

class Tokenizer {
public:
    explicit Tokenizer(std::string_view source) : source_(source) {}
    [[nodiscard]] std::vector<Token> tokenize() const;

private:
    std::string_view source_;
};

} // namespace serdetk::xpath
