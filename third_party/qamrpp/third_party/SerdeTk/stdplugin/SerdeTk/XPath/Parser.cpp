#include "Parser.hpp"

#include "SerdeTk.hpp"

namespace serdetk::xpath {

CodeTreePtr Parser::parse(std::string_view expression) const {
    State state(Tokenizer(expression).tokenize());
    auto tree = state.expression();
    if (!state.at_end())
        throw ParseError("err:XPST0003: unexpected trailing XPath token");
    return tree;
}

const Token& Parser::State::current() const { return tokens_[position_]; }
bool Parser::State::at_end() const { return current().kind == TokenKind::End; }

bool Parser::State::accept(TokenKind kind) {
    if (current().kind != kind) return false;
    ++position_;
    return true;
}

const Token& Parser::State::expect(TokenKind kind, std::string_view message) {
    if (current().kind != kind)
        throw ParseError("err:XPST0003: " + std::string(message) + " at offset " + std::to_string(current().offset));
    return tokens_[position_++];
}

CodeTreePtr Parser::State::expression() { return binary(1); }

unsigned Parser::State::precedence(TokenKind kind) const {
    switch (kind) {
    case TokenKind::KeywordOr: return 1;
    case TokenKind::KeywordAnd: return 2;
    case TokenKind::Equal: case TokenKind::NotEqual: case TokenKind::Less: case TokenKind::LessEqual:
    case TokenKind::Greater: case TokenKind::GreaterEqual: return 3;
    case TokenKind::Plus: case TokenKind::Minus: return 4;
    case TokenKind::Star: case TokenKind::KeywordDiv: case TokenKind::KeywordIdiv: case TokenKind::KeywordMod: return 5;
    case TokenKind::Pipe: return 6;
    default: return 0;
    }
}

CodeTreePtr Parser::State::binary(unsigned minimum) {
    auto left = unary();
    for (;;) {
        const unsigned rank = precedence(current().kind);
        if (rank < minimum) break;
        const auto operation = current().lexeme;
        ++position_;
        auto right = binary(rank + 1);
        auto node = CodeTree::node(NodeKind::Binary, operation);
        node->children = {std::move(left), std::move(right)};
        left = std::move(node);
    }
    return left;
}

CodeTreePtr Parser::State::unary() {
    if (accept(TokenKind::Plus) || accept(TokenKind::Minus)) {
        const auto operation = tokens_[position_ - 1].lexeme;
        auto node = CodeTree::node(NodeKind::Unary, operation);
        node->children.push_back(unary());
        return node;
    }
    return primary();
}

CodeTreePtr Parser::State::path(bool absolute) {
    auto node = CodeTree::node(NodeKind::Path, absolute ? "absolute" : "relative");
    bool need_step = true;
    while (need_step) {
        PathStep step;
        if (accept(TokenKind::DoubleSlash)) {
            step.kind = StepKind::Descendant;
            if (accept(TokenKind::Star)) step.name = "*";
            else step.name = expect(TokenKind::Name, "expected descendant name").lexeme;
        } else if (accept(TokenKind::At)) {
            step.kind = StepKind::Attribute;
            step.name = accept(TokenKind::Star) ? "*" : expect(TokenKind::Name, "expected attribute name").lexeme;
        } else if (accept(TokenKind::Question)) {
            step.kind = StepKind::Lookup;
            if (accept(TokenKind::Star)) step.name = "*";
            else if (current().kind == TokenKind::Number || current().kind == TokenKind::String || current().kind == TokenKind::Name)
                step.name = tokens_[position_++].lexeme;
            else throw ParseError("err:XPST0003: expected lookup key");
        } else if (accept(TokenKind::Star)) {
            step.kind = StepKind::Wildcard;
            step.name = "*";
        } else if (current().kind == TokenKind::Name) {
            step.kind = StepKind::Child;
            step.name = tokens_[position_++].lexeme;
        } else {
            if (need_step) throw ParseError("err:XPST0003: expected path step");
            break;
        }
        predicates(step);
        node->steps.push_back(std::move(step));
        if (accept(TokenKind::Slash)) {
            need_step = true;
            continue;
        }
        if (current().kind == TokenKind::Question) {
            need_step = true;
            continue;
        }
        break;
    }
    return node;
}

void Parser::State::predicates(PathStep& step) {
    while (accept(TokenKind::LBracket)) {
        step.predicates.push_back(expression());
        expect(TokenKind::RBracket, "expected ']'");
    }
}

CodeTreePtr Parser::State::primary() {
    if (accept(TokenKind::Slash)) {
        if (current().kind == TokenKind::End || current().kind == TokenKind::RParen) return CodeTree::node(NodeKind::Root);
        return path(true);
    }
    if (current().kind == TokenKind::DoubleSlash) {
        auto node = CodeTree::node(NodeKind::Path, "absolute");
        PathStep step;
        step.kind = StepKind::Descendant;
        ++position_;
        step.name = accept(TokenKind::Star) ? "*" : expect(TokenKind::Name, "expected descendant name").lexeme;
        predicates(step);
        node->steps.push_back(std::move(step));
        return node;
    }
    if (current().kind == TokenKind::Name && position_ + 1 < tokens_.size() && tokens_[position_ + 1].kind == TokenKind::LParen) {
        const auto name = tokens_[position_++].lexeme;
        expect(TokenKind::LParen, "expected '('");
        auto node = CodeTree::node(NodeKind::Function, name);
        if (!accept(TokenKind::RParen)) {
            do node->children.push_back(expression()); while (accept(TokenKind::Comma));
            expect(TokenKind::RParen, "expected ')'");
        }
        return node;
    }
    if (current().kind == TokenKind::String || current().kind == TokenKind::Number) return CodeTree::literal(tokens_[position_++].lexeme);
    if (current().kind == TokenKind::Name && (current().lexeme == "true" || current().lexeme == "false" || current().lexeme == "null"))
        return CodeTree::literal(tokens_[position_++].lexeme);
    if (accept(TokenKind::Dot)) return CodeTree::node(NodeKind::ContextItem);
    if (accept(TokenKind::LParen)) {
        auto result = expression();
        expect(TokenKind::RParen, "expected ')'");
        return result;
    }
    if (accept(TokenKind::LBracket)) {
        auto node = CodeTree::node(NodeKind::ArrayConstructor);
        if (!accept(TokenKind::RBracket)) {
            do node->children.push_back(expression()); while (accept(TokenKind::Comma));
            expect(TokenKind::RBracket, "expected ']'");
        }
        return node;
    }
    if (current().kind == TokenKind::Name && current().lexeme == "map" && position_ + 1 < tokens_.size() && tokens_[position_ + 1].kind == TokenKind::LBrace) {
        ++position_;
        expect(TokenKind::LBrace, "expected '{'");
        auto node = CodeTree::node(NodeKind::MapConstructor);
        if (!accept(TokenKind::RBrace)) {
            do {
                auto key = expression();
                expect(TokenKind::Colon, "expected ':'");
                node->entries.emplace_back(std::move(key), expression());
            } while (accept(TokenKind::Comma));
            expect(TokenKind::RBrace, "expected '}'");
        }
        return node;
    }
    if (current().kind == TokenKind::Name || current().kind == TokenKind::Star || current().kind == TokenKind::At || current().kind == TokenKind::Question)
        return path(false);
    throw ParseError("err:XPST0003: expected expression at offset " + std::to_string(current().offset));
}

} // namespace serdetk::xpath
