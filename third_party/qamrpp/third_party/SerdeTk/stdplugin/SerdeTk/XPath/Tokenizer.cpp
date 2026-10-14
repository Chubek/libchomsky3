#include "Tokenizer.hpp"

#include "SerdeTk.hpp"

#include <cctype>

namespace serdetk::xpath {
namespace {

bool name_start(char value) {
    return std::isalpha(static_cast<unsigned char>(value)) || value == '_' || value == '*';
}

bool name_continue(char value) {
    return std::isalnum(static_cast<unsigned char>(value)) || value == '_' || value == '-' || value == '.' || value == ':';
}

TokenKind keyword_kind(std::string_view text) {
    if (text == "and") return TokenKind::KeywordAnd;
    if (text == "or") return TokenKind::KeywordOr;
    if (text == "div") return TokenKind::KeywordDiv;
    if (text == "idiv") return TokenKind::KeywordIdiv;
    if (text == "mod") return TokenKind::KeywordMod;
    return TokenKind::Name;
}

} // namespace

std::vector<Token> Tokenizer::tokenize() const {
    std::vector<Token> out;
    for (std::size_t index = 0; index < source_.size();) {
        const char current = source_[index];
        if (std::isspace(static_cast<unsigned char>(current))) {
            ++index;
            continue;
        }
        const auto emit = [&](TokenKind kind, std::size_t size = 1) {
            out.push_back({kind, std::string(source_.substr(index, size)), index});
            index += size;
        };
        if (current == '(' && index + 1 < source_.size() && source_[index + 1] == ':') {
            index += 2;
            const auto end = source_.find(":)", index);
            if (end == std::string_view::npos) throw ParseError("err:XPST0003: unclosed XPath comment");
            index = end + 2;
            continue;
        }
        if (current == '\'' || current == '"') {
            const char quote = current;
            const auto start = index++;
            std::string value;
            while (index < source_.size()) {
                if (source_[index] == quote) {
                    if (index + 1 < source_.size() && source_[index + 1] == quote) {
                        value.push_back(quote);
                        index += 2;
                        continue;
                    }
                    ++index;
                    out.push_back({TokenKind::String, std::move(value), start});
                    break;
                }
                value.push_back(source_[index++]);
            }
            if (index > source_.size() || (index == source_.size() && source_[index - 1] != quote))
                throw ParseError("err:XPST0003: unclosed string literal");
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(current)) ||
            (current == '.' && index + 1 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[index + 1])))) {
            const auto start = index++;
            while (index < source_.size() &&
                   (std::isdigit(static_cast<unsigned char>(source_[index])) || source_[index] == '.' ||
                    source_[index] == 'e' || source_[index] == 'E' || source_[index] == '+' || source_[index] == '-')) ++index;
            out.push_back({TokenKind::Number, std::string(source_.substr(start, index - start)), start});
            continue;
        }
        if (name_start(current)) {
            if (current == '*') {
                emit(TokenKind::Star);
                continue;
            }
            const auto start = index++;
            while (index < source_.size() && name_continue(source_[index])) ++index;
            const auto text = source_.substr(start, index - start);
            out.push_back({keyword_kind(text), std::string(text), start});
            continue;
        }
        if (current == '/' && index + 1 < source_.size() && source_[index + 1] == '/') emit(TokenKind::DoubleSlash, 2);
        else if (current == '.' && index + 1 < source_.size() && source_[index + 1] == '.') emit(TokenKind::DotDot, 2);
        else if (current == '!' && index + 1 < source_.size() && source_[index + 1] == '=') emit(TokenKind::NotEqual, 2);
        else if (current == '<' && index + 1 < source_.size() && source_[index + 1] == '=') emit(TokenKind::LessEqual, 2);
        else if (current == '>' && index + 1 < source_.size() && source_[index + 1] == '=') emit(TokenKind::GreaterEqual, 2);
        else if (current == ':' && index + 1 < source_.size() && source_[index + 1] == '=') emit(TokenKind::Assign, 2);
        else {
            const TokenKind kind = [&] {
                switch (current) {
                case '/': return TokenKind::Slash; case '.': return TokenKind::Dot; case '@': return TokenKind::At;
                case '?': return TokenKind::Question; case '(': return TokenKind::LParen; case ')': return TokenKind::RParen;
                case '[': return TokenKind::LBracket; case ']': return TokenKind::RBracket; case '{': return TokenKind::LBrace;
                case '}': return TokenKind::RBrace; case ',': return TokenKind::Comma; case ':': return TokenKind::Colon;
                case '+': return TokenKind::Plus; case '-': return TokenKind::Minus; case '=': return TokenKind::Equal;
                case '<': return TokenKind::Less; case '>': return TokenKind::Greater; case '|': return TokenKind::Pipe;
                default: throw ParseError("err:XPST0003: invalid XPath character at offset " + std::to_string(index));
                }
            }();
            emit(kind);
        }
    }
    out.push_back({TokenKind::End, {}, source_.size()});
    return out;
}

} // namespace serdetk::xpath
