#include "IR.hpp"

#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace jiterati::ir {
namespace {

// ---------------------------------------------------------------------------
// Lexer
//
// The terse text format is whitespace-delimited with parentheses as the only
// structural punctuation, so tokens are either '(', ')', or a "word" run of
// characters up to the next whitespace, paren, or ';'.  Line comments start
// with ';' (handy for authored source; the printer never emits them).
// ---------------------------------------------------------------------------

enum class TokKind {
    End,
    LParen,
    RParen,
    Word,
};

struct Token {
    TokKind kind = TokKind::End;
    std::string text;
    std::uint32_t line = 1;
};

bool is_break(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
           c == '\f' || c == '\v' || c == '(' || c == ')' || c == ';';
}

std::vector<Token> lex(std::string const& src, std::vector<Diagnostic>* diag) {
    std::vector<Token> out;
    std::uint32_t line = 1;
    std::size_t i = 0;
    while (i < src.size()) {
        char c = src[i];
        if (c == '\n') { ++line; ++i; continue; }
        if (c == ';') { // line comment
            while (i < src.size() && src[i] != '\n') ++i;
            continue;
        }
        Token t;
        t.line = line;
        if (c == '(') { t.kind = TokKind::LParen; t.text = "("; ++i; out.push_back(std::move(t)); continue; }
        if (c == ')') { t.kind = TokKind::RParen; t.text = ")"; ++i; out.push_back(std::move(t)); continue; }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') { ++i; continue; }
        std::size_t start = i;
        while (i < src.size() && !is_break(src[i])) ++i;
        t.kind = TokKind::Word;
        t.text = src.substr(start, i - start);
        out.push_back(std::move(t));
    }
    Token end;
    end.kind = TokKind::End;
    end.line = line > 0 ? line : 1;
    out.push_back(std::move(end));
    (void)diag;
    return out;
}

// ---------------------------------------------------------------------------
// Token classification tables
// ---------------------------------------------------------------------------

const std::set<std::string>& binary_operators() {
    static const std::set<std::string> ops = {
        "+", "-", "*", "s/", "u/", "s%", "u%", "&", "|", "^",
        "<<", "l>>", "a>>", "==", "!=",
        "s<", "s<=", "s>", "s>=", "u<", "u<=", "u>", "u>=",
    };
    return ops;
}

bool binary_op_from_string(std::string const& s, BinaryOp& out) {
    static const std::map<std::string, BinaryOp> table = {
        {"+", BinaryOp::Add}, {"-", BinaryOp::Sub}, {"*", BinaryOp::Mul},
        {"s/", BinaryOp::SDiv}, {"u/", BinaryOp::UDiv},
        {"s%", BinaryOp::SRem}, {"u%", BinaryOp::URem},
        {"&", BinaryOp::And}, {"|", BinaryOp::Or}, {"^", BinaryOp::Xor},
        {"<<", BinaryOp::Shl}, {"l>>", BinaryOp::LShr}, {"a>>", BinaryOp::AShr},
        {"==", BinaryOp::Eq}, {"!=", BinaryOp::Ne},
        {"s<", BinaryOp::SLt}, {"s<=", BinaryOp::SLe},
        {"s>", BinaryOp::SGt}, {"s>=", BinaryOp::SGe},
        {"u<", BinaryOp::ULt}, {"u<=", BinaryOp::ULe},
        {"u>", BinaryOp::UGt}, {"u>=", BinaryOp::UGe},
    };
    auto it = table.find(s);
    if (it == table.end()) return false;
    out = it->second;
    return true;
}

bool unary_op_from_string(std::string const& s, UnaryOp& out) {
    if (s == "neg") { out = UnaryOp::Neg; return true; }
    if (s == "not") { out = UnaryOp::Not; return true; }
    return false;
}

bool is_comparison(BinaryOp op) {
    switch (op) {
        case BinaryOp::Eq: case BinaryOp::Ne:
        case BinaryOp::SLt: case BinaryOp::SLe: case BinaryOp::SGt: case BinaryOp::SGe:
        case BinaryOp::ULt: case BinaryOp::ULe: case BinaryOp::UGt: case BinaryOp::UGe:
            return true;
        default:
            return false;
    }
}

bool type_from_string(std::string const& s, Type& out) {
    if (s == "void") { out = Type::void_ty(); return true; }
    if (s == "bool") { out = Type::bool_ty(); return true; }
    if (s == "i32") { out = Type::i32(); return true; }
    if (s == "i64") { out = Type::i64(); return true; }
    if (s == "f32") { out = Type::f32(); return true; }
    if (s == "f64") { out = Type::f64(); return true; }
    if (s == "ptr") { out = Type::ptr(); return true; }
    return false;
}

bool number_from_string(std::string const& s, Immediate& imm, Type& ty) {
    if (s.empty()) return false;
    bool is_float = s.find('.') != std::string::npos;
    if (!is_float) {
        for (char c : s) if (c == 'e' || c == 'E') { is_float = true; break; }
    }
    try {
        if (is_float) {
            std::size_t used = 0;
            double v = std::stod(s, &used);
            if (used != s.size()) return false;
            imm = v;
            ty = Type::f64();
            return true;
        }
        std::size_t used = 0;
        long long v = std::stoll(s, &used, 10);
        if (used != s.size()) return false;
        imm = static_cast<std::int64_t>(v);
        ty = Type::i64();
        return true;
    } catch (...) {
        return false;
    }
}

bool looks_like_identifier(std::string const& s) {
    if (s.empty()) return false;
    if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
    for (char c : s) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Recursive-descent parser
// ---------------------------------------------------------------------------

struct FunctionSignature {
    TerseFunction proto;
    std::size_t body_start = 0; // token index of the first body token
};

class Parser {
public:
    Parser(std::vector<Token> toks, std::vector<Diagnostic>* diag)
        : toks_(std::move(toks)), diag_(diag) {}

    std::optional<TerseModule> parse() {
        if (!parse_header(module_)) return fail("failed to parse terse module header");

        // Pass 1: collect every function signature so call results can resolve
        // their return type even when the callee is declared later in the file.
        std::size_t guard = 0;
        while (peek().kind != TokKind::End) {
            if (++guard > 1'000'000) return fail("runaway function list");
            FunctionSignature sig;
            if (!parse_signature(sig.proto, sig.body_start)) return std::nullopt;
            return_types_[sig.proto.name] = sig.proto.return_type;
            signatures_.push_back(std::move(sig));
            skip_body();
        }

        // Pass 2: parse bodies now that the full signature table is known.
        for (auto& sig : signatures_) {
            pos_ = sig.body_start;
            params_.clear();
            for (std::uint32_t i = 0; i < sig.proto.parameters.size(); ++i) {
                params_[sig.proto.parameters[i].name] = sig.proto.parameters[i].type;
            }
            auto body = parse_expr();
            if (!body) return std::nullopt;
            if (peek().kind == TokKind::RParen) {
                return fail_at("unexpected ')' in function body", peek());
            }
            sig.proto.body = std::move(*body);
            module_.functions.push_back(std::move(sig.proto));
        }
        return module_;
    }

private:
    Token const& peek() const { return toks_[pos_]; }
    Token const& advance() { return toks_[pos_++]; }

    bool accept(TokKind k) {
        if (peek().kind == k) { ++pos_; return true; }
        return false;
    }

    bool expect(TokKind k, char const* what) {
        if (peek().kind == k) { ++pos_; return true; }
        return failb_at(std::string("expected ") + what, peek());
    }

    bool expect_word(char const* w) {
        if (peek().kind == TokKind::Word && peek().text == w) { ++pos_; return true; }
        return failb_at(std::string("expected '") + w + "'", peek());
    }

    void emit_error(std::string message, Token const& at) {
        if (diag_ != nullptr) {
            diag_->push_back({ Diagnostic::Severity::Error, std::move(message),
                               module_.name, at.line });
        }
    }

    std::nullopt_t fail(std::string message) {
        emit_error(std::move(message), peek());
        return std::nullopt;
    }

    std::nullopt_t fail_at(std::string message, Token const& at) {
        emit_error(std::move(message), at);
        return std::nullopt;
    }

    bool failb(std::string message) {
        emit_error(std::move(message), peek());
        return false;
    }

    bool failb_at(std::string message, Token const& at) {
        emit_error(std::move(message), at);
        return false;
    }

    bool parse_header(TerseModule& m) {
        if (peek().kind != TokKind::Word || peek().text != "terse") {
            emit_error("expected 'terse module'", peek());
            return false;
        }
        ++pos_;
        if (!expect_word("module")) return false;
        if (peek().kind != TokKind::Word) return failb_at("expected module name", peek());
        m.name = advance().text;
        return true;
    }

    // Parse `fn name(params) -> type =`, leaving the body for pass 2.  The
    // printer emits `name: type` pairs joined by `, `, so the ':' glues to the
    // parameter name and the ',' glues to the preceding type; both are trimmed
    // here, and the spaced-out `x : t` / `t , y` forms are accepted too.
    bool parse_signature(TerseFunction& f, std::size_t& body_start) {
        if (!expect_word("fn")) return false;
        if (peek().kind != TokKind::Word) return failb_at("expected function name", peek());
        f.name = advance().text;
        if (!expect(TokKind::LParen, "'(' to open parameter list")) return false;

        while (peek().kind == TokKind::Word) {
            Parameter p;
            Token name_tok = advance();
            if (!name_tok.text.empty() && name_tok.text.back() == ':') {
                p.name = name_tok.text.substr(0, name_tok.text.size() - 1);
            } else {
                p.name = name_tok.text;
                if (peek().kind == TokKind::Word && peek().text == ":") ++pos_;
            }
            if (p.name.empty()) return failb_at("expected parameter name", name_tok);

            if (peek().kind != TokKind::Word) return failb_at("expected parameter type", peek());
            Token type_tok = advance();
            std::string type_text = type_tok.text;
            bool more = false;
            if (!type_text.empty() && type_text.back() == ',') {
                type_text.pop_back();
                more = true;
            }
            if (!type_from_string(type_text, p.type)) {
                return failb_at("unknown type '" + type_text + "'", type_tok);
            }
            f.parameters.push_back(std::move(p));
            if (!more && peek().kind == TokKind::Word && peek().text == ",") { ++pos_; more = true; }
            if (!more) break;
        }

        if (!expect(TokKind::RParen, "')' to close parameter list")) return false;
        if (!expect_word("->")) return false;
        if (peek().kind != TokKind::Word) return failb_at("expected return type", peek());
        Token ret_tok = advance();
        if (!type_from_string(ret_tok.text, f.return_type)) {
            return failb_at("unknown return type '" + ret_tok.text + "'", ret_tok);
        }
        if (!expect_word("=")) return false;
        body_start = pos_;
        return true;
    }

    // Advance over one balanced expression without interpreting it (pass 1).
    void skip_body() { skip_expr(); }

    bool skip_expr() {
        Token const& t = peek();
        if (t.kind == TokKind::End) return false;
        if (t.kind == TokKind::Word) { ++pos_; return true; }
        if (t.kind == TokKind::LParen) {
            ++pos_;
            while (peek().kind == TokKind::Word || peek().kind == TokKind::LParen) {
                if (!skip_expr()) return false;
            }
            if (peek().kind == TokKind::RParen) ++pos_;
            return true;
        }
        return false;
    }

    std::optional<TerseExpr> parse_expr() {
        Token const& t = peek();
        if (t.kind == TokKind::End) return fail_at("unexpected end of input", t);
        if (t.kind == TokKind::RParen) return fail_at("unexpected ')'", t);

        if (t.kind == TokKind::LParen) {
            ++pos_; // consume '('
            if (peek().kind != TokKind::Word) return fail_at("expected form head after '('", peek());
            Token head = advance();
            std::optional<TerseExpr> result;
            if (head.text == "let") result = parse_let();
            else if (head.text == "if") result = parse_if();
            else if (head.text == "neg" || head.text == "not") result = parse_unary(head.text);
            else if (binary_operators().count(head.text)) result = parse_binary(head.text);
            else result = parse_call(head.text);
            if (!result) return std::nullopt;
            if (!expect(TokKind::RParen, "')' to close form")) return std::nullopt;
            return result;
        }

        // Atom: literal or argument reference.
        return parse_atom(t.text, t);
    }

    std::optional<TerseExpr> parse_atom(std::string const& text, Token const& at) {
        ++pos_;
        if (text == "undef") return TerseExpr::literal_of(Type::void_ty(), Immediate{});
        if (text == "true") return TerseExpr::literal_of(Type::bool_ty(), Immediate{ true });
        if (text == "false") return TerseExpr::literal_of(Type::bool_ty(), Immediate{ false });
        if (text == "null") return TerseExpr::literal_of(Type::ptr(), Immediate{ nullptr });
        Immediate imm;
        Type ty;
        if (number_from_string(text, imm, ty)) return TerseExpr::literal_of(ty, imm);
        if (!looks_like_identifier(text)) return fail_at("invalid token '" + text + "'", at);
        Type ty2 = Type::i64();
        auto it = params_.find(text);
        if (it != params_.end()) ty2 = it->second;
        return TerseExpr::argument(ty2, text);
    }

    std::optional<TerseExpr> parse_unary(std::string const& head) {
        UnaryOp uop{};
        if (!unary_op_from_string(head, uop)) return fail("invalid unary operator");
        auto operand = parse_expr();
        if (!operand) return std::nullopt;
        return TerseExpr::unary_expr(operand->type, uop, std::move(*operand));
    }

    std::optional<TerseExpr> parse_binary(std::string const& head) {
        BinaryOp bop{};
        if (!binary_op_from_string(head, bop)) return fail("invalid binary operator");
        auto lhs = parse_expr();
        if (!lhs) return std::nullopt;
        auto rhs = parse_expr();
        if (!rhs) return std::nullopt;
        Type ty = is_comparison(bop) ? Type::bool_ty() : lhs->type;
        return TerseExpr::binary_expr(ty, bop, std::move(*lhs), std::move(*rhs));
    }

    std::optional<TerseExpr> parse_call(std::string const& callee) {
        if (!looks_like_identifier(callee)) return fail("invalid call target '" + callee + "'");
        std::vector<TerseExpr> args;
        while (peek().kind == TokKind::Word || peek().kind == TokKind::LParen) {
            auto arg = parse_expr();
            if (!arg) return std::nullopt;
            args.push_back(std::move(*arg));
        }
        Type ret = Type::i64();
        auto it = return_types_.find(callee);
        if (it != return_types_.end()) ret = it->second;
        return TerseExpr::call(ret, callee, std::move(args));
    }

    std::optional<TerseExpr> parse_let() {
        if (peek().kind != TokKind::Word) return fail_at("expected binding name in let", peek());
        std::string name = advance().text;
        auto value = parse_expr();
        if (!value) return std::nullopt;
        auto old = params_.find(name);
        bool had_old = old != params_.end();
        Type old_type = had_old ? old->second : Type::void_ty();
        params_[name] = value->type;
        auto body = parse_expr();
        if (!body) return std::nullopt;
        if (had_old) params_[name] = old_type;
        else params_.erase(name);
        return TerseExpr::let(name, std::move(*value), std::move(*body));
    }

    std::optional<TerseExpr> parse_if() {
        auto cond = parse_expr();
        if (!cond) return std::nullopt;
        auto true_branch = parse_expr();
        if (!true_branch) return std::nullopt;
        auto false_branch = parse_expr();
        if (!false_branch) return std::nullopt;
        return TerseExpr::if_expr(true_branch->type, std::move(*cond),
                                  std::move(*true_branch), std::move(*false_branch));
    }

    std::vector<Token> toks_;
    std::vector<Diagnostic>* diag_;
    std::size_t pos_ = 0;
    TerseModule module_;
    std::vector<FunctionSignature> signatures_;
    std::map<std::string, Type> return_types_;
    std::map<std::string, Type> params_;
};

} // namespace

std::optional<TerseModule> parse_terse_module(std::string const& source, std::vector<Diagnostic>* diagnostics) {
    std::vector<Token> tokens = lex(source, diagnostics);
    Parser parser(std::move(tokens), diagnostics);
    return parser.parse();
}

} // namespace jiterati::ir
