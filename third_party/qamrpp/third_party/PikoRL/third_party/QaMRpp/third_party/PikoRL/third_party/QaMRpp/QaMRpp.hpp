/* QaMRpp.hpp - Minimal Header-Only Tree-Walking Interpreter */

#ifndef QAMRPP_HPP
#define QAMRPP_HPP

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <functional>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "QaMRpp-Plugin.hpp"

extern "C" {
qamrpp_value* qamrpp_value_nil(qamrpp_context*);
qamrpp_value* qamrpp_value_bool(qamrpp_context*, int);
qamrpp_value* qamrpp_value_int(qamrpp_context*, int64_t);
qamrpp_value* qamrpp_value_float(qamrpp_context*, double);
qamrpp_value* qamrpp_value_string(qamrpp_context*, const char*, size_t);
qamrpp_value* qamrpp_value_userdata(qamrpp_context*, void*, void (*)(void*));
qamrpp_value* qamrpp_value_table(qamrpp_context*);
qamrpp_value_type qamrpp_value_get_type(const qamrpp_value*);
int qamrpp_value_as_bool(const qamrpp_value*);
int64_t qamrpp_value_as_int(const qamrpp_value*);
double qamrpp_value_as_float(const qamrpp_value*);
const char* qamrpp_value_as_string(const qamrpp_value*, size_t*);
void* qamrpp_value_as_userdata(const qamrpp_value*);
qamrpp_value* qamrpp_table_raw_get(qamrpp_context*, qamrpp_value*, qamrpp_value*);
int qamrpp_table_raw_set(qamrpp_context*, qamrpp_value*, qamrpp_value*, qamrpp_value*);
qamrpp_value* qamrpp_table_get(qamrpp_context*, qamrpp_value*, qamrpp_value*);
int qamrpp_table_set(qamrpp_context*, qamrpp_value*, qamrpp_value*, qamrpp_value*);
qamrpp_value* qamrpp_value_get_metatable(qamrpp_context*, qamrpp_value*);
int qamrpp_value_set_metatable(qamrpp_context*, qamrpp_value*, qamrpp_value*);
void qamrpp_set_error(qamrpp_context*, qamrpp_error_code, const char*);
void* qamrpp_context_get_userdata(qamrpp_context*);
void qamrpp_context_set_userdata(qamrpp_context*, void*);
qamrpp_value* qamrpp_get_global(qamrpp_context*, const char*);
void qamrpp_set_global(qamrpp_context*, const char*, qamrpp_value*);
}

namespace qamrpp {
class Context;
namespace stdlib {
void load_core(Context& ctx);
void load_string(Context& ctx);
void load_table(Context& ctx);
void load_math(Context& ctx);
void load_io(Context& ctx);
void load_os(Context& ctx);
void load_debug(Context& ctx);
void load_coroutine(Context& ctx);
void load_package(Context& ctx);
void load_utf8(Context& ctx);
} // namespace stdlib

/* ============================================================
 * Forward declarations
 * ============================================================ */

class Context;
class Value;
class Extension;

struct Node;

using ValuePtr = std::shared_ptr<Value>;
using NodePtr  = std::shared_ptr<Node>;

using NativeFn =
    std::function<ValuePtr(Context&, std::vector<ValuePtr>&)>;

/* ============================================================
 * Value
 * ============================================================ */

class Value {
public:
    enum Type {
        NIL,
        BOOL,
        INT,
        FLOAT,
        STRING,
        FUNCTION,
        USERDATA,
        TABLE
    };

    Type type = NIL;

    bool bool_value = false;
    int64_t int_value = 0;
    double float_value = 0.0;

    std::string string_value;

    NativeFn function_value;

    void* userdata_value = nullptr;
    std::function<void(void*)> userdata_destructor;
    std::vector<std::pair<ValuePtr, ValuePtr>> table_entries;
    ValuePtr metatable_value;

    Value() = default;

    explicit Value(bool v)
        : type(BOOL), bool_value(v) {}

    explicit Value(int64_t v)
        : type(INT), int_value(v) {}

    explicit Value(double v)
        : type(FLOAT), float_value(v) {}

    explicit Value(std::string v)
        : type(STRING), string_value(std::move(v)) {}

    explicit Value(NativeFn fn)
        : type(FUNCTION), function_value(std::move(fn)) {}

    static ValuePtr make_table() {
        auto table = std::make_shared<Value>();
        table->type = TABLE;
        return table;
    }

    ~Value() {
        if (type == USERDATA &&
            userdata_value &&
            userdata_destructor) {
            userdata_destructor(userdata_value);
        }
    }

    bool truthy() const {
        switch (type) {
            case NIL:
                return false;

            case BOOL:
                return bool_value;

            case INT:
                return int_value != 0;

            case FLOAT:
                return float_value != 0.0;

            case STRING:
                return !string_value.empty();

            default:
                return true;
        }
    }

    std::string to_string() const {
        switch (type) {
            case NIL:
                return "nil";

            case BOOL:
                return bool_value ? "true" : "false";

            case INT:
                return std::to_string(int_value);

            case FLOAT: {
                std::ostringstream ss;
                ss << float_value;
                return ss.str();
            }

            case STRING:
                return string_value;

            case FUNCTION:
                return "<function>";

            case USERDATA:
                return "<userdata>";

            case TABLE:
                return "<table>";
        }

        return "<unknown>";
    }
};

/* ============================================================
 * AST
 * ============================================================ */

struct Node {
    enum Type {
        NODE_LITERAL,
        NODE_IDENT,
        NODE_BINARY,
        NODE_UNARY,
        NODE_CALL,
        NODE_INDEX,
        NODE_TABLE,
        NODE_ASSIGN,
        NODE_LOCAL_ASSIGN,
        NODE_IF,
        NODE_WHILE,
        NODE_FOR_NUMERIC,
        NODE_FOR_GENERIC,
        NODE_RETURN,
        NODE_FUNCTION_DEF,
        NODE_FUNCTION_EXPR,
        NODE_MULTI_ASSIGN,
        NODE_MULTI_RETURN,
        NODE_BLOCK
    };

    Type type;

    std::string text;

    ValuePtr literal;

    std::vector<NodePtr> children;

    explicit Node(Type t)
        : type(t) {}
};

/* ============================================================
 * Extension interface
 * ============================================================ */

class Extension {
public:
    virtual ~Extension() = default;

    virtual const char* name() const = 0;

    virtual void register_functions(Context& ctx) = 0;
};

class Linker {
public:
    struct Unit {
        std::string path;
        std::string source;
    };

private:
    std::vector<Unit> units_;

public:
    void clear() { units_.clear(); }
    size_t size() const { return units_.size(); }

    void add_source(std::string path, std::string source) {
        Unit u;
        u.path = std::move(path);
        u.source = std::move(source);
        units_.push_back(std::move(u));
    }

    bool add_file(const std::string& path) {
        std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
        if (!in.good()) {
            return false;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        add_source(path, ss.str());
        return true;
    }

    ValuePtr link(Context& ctx) const;
};

enum class StdLib : uint32_t {
    NONE      = 0,
    CORE      = 1u << 0,
    STRING    = 1u << 1,
    TABLE     = 1u << 2,
    MATH      = 1u << 3,
    IO        = 1u << 4,
    OS        = 1u << 5,
    DEBUGLIB  = 1u << 6,
    COROUTINE = 1u << 7,
    PACKAGE   = 1u << 8,
    UTF8      = 1u << 9,
    ALL       = 0xFFFFFFFFu
};

inline StdLib operator|(StdLib a, StdLib b) {
    return static_cast<StdLib>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline StdLib operator&(StdLib a, StdLib b) {
    return static_cast<StdLib>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/* ============================================================
 * Lexer
 * ============================================================ */

class Lexer {
public:
    enum TokenType {
        TOK_EOF,

        TOK_IDENT,
        TOK_INT,
        TOK_FLOAT,
        TOK_STRING,

        TOK_PLUS,
        TOK_MINUS,
        TOK_STAR,
        TOK_SLASH,
        TOK_LT,
        TOK_GT,
        TOK_LE,
        TOK_GE,
        TOK_EQ,
        TOK_NE,

        TOK_LPAREN,
        TOK_RPAREN,
        TOK_LBRACE,
        TOK_RBRACE,
        TOK_LBRACKET,
        TOK_RBRACKET,
        TOK_DOT,
        TOK_COLON,

        TOK_COMMA,
        TOK_EQUAL,
        TOK_SEMICOLON
    };

    struct Token {
        TokenType type;
        std::string text;
    };

private:
    std::string source;
    size_t pos = 0;

public:
    explicit Lexer(std::string src)
        : source(std::move(src)) {}

    Token next() {
        skip_whitespace();

        if (pos >= source.size()) {
            return {TOK_EOF, ""};
        }

        char c = source[pos];

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            return ident();
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            return number();
        }

        switch (c) {
            case '+':
                ++pos;
                return {TOK_PLUS, "+"};

            case '-':
                ++pos;
                return {TOK_MINUS, "-"};

            case '*':
                ++pos;
                return {TOK_STAR, "*"};

            case '/':
                ++pos;
                return {TOK_SLASH, "/"};
            case '<':
                ++pos;
                if (pos < source.size() && source[pos] == '=') {
                    ++pos;
                    return {TOK_LE, "<="};
                }
                return {TOK_LT, "<"};
            case '>':
                ++pos;
                if (pos < source.size() && source[pos] == '=') {
                    ++pos;
                    return {TOK_GE, ">="};
                }
                return {TOK_GT, ">"};

            case '(':
                ++pos;
                return {TOK_LPAREN, "("};

            case ')':
                ++pos;
                return {TOK_RPAREN, ")"};

            case '{':
                ++pos;
                return {TOK_LBRACE, "{"};

            case '}':
                ++pos;
                return {TOK_RBRACE, "}"};

            case '[':
                ++pos;
                return {TOK_LBRACKET, "["};

            case ']':
                ++pos;
                return {TOK_RBRACKET, "]"};

            case '.':
                ++pos;
                return {TOK_DOT, "."};

            case ':':
                ++pos;
                return {TOK_COLON, ":"};

            case ',':
                ++pos;
                return {TOK_COMMA, ","};

            case '=':
                ++pos;
                if (pos < source.size() && source[pos] == '=') {
                    ++pos;
                    return {TOK_EQ, "=="};
                }
                return {TOK_EQUAL, "="};
            case '~':
                ++pos;
                if (pos < source.size() && source[pos] == '=') {
                    ++pos;
                    return {TOK_NE, "~="};
                }
                throw std::runtime_error("unexpected character: ~");

            case ';':
                ++pos;
                return {TOK_SEMICOLON, ";"};

            case '"':
                return string();

            default:
                throw std::runtime_error(
                    std::string("unexpected character: ") + c
                );
        }
    }

private:
    void skip_whitespace() {
        while (pos < source.size() &&
               std::isspace(static_cast<unsigned char>(source[pos]))) {
            ++pos;
        }
    }

    Token ident() {
        size_t start = pos;

        while (pos < source.size()) {
            char c = source[pos];

            if (!std::isalnum(static_cast<unsigned char>(c)) &&
                c != '_') {
                break;
            }

            ++pos;
        }

        return {
            TOK_IDENT,
            source.substr(start, pos - start)
        };
    }

    Token number() {
        size_t start = pos;
        bool is_float = false;

        while (pos < source.size()) {
            char c = source[pos];

            if (c == '.') {
                if (is_float) {
                    throw std::runtime_error("invalid number literal");
                }
                is_float = true;
                ++pos;
                continue;
            }

            if (!std::isdigit(static_cast<unsigned char>(c))) {
                break;
            }

            ++pos;
        }

        return {
            is_float ? TOK_FLOAT : TOK_INT,
            source.substr(start, pos - start)
        };
    }

    Token string() {
        ++pos;

        size_t start = pos;

        while (pos < source.size() &&
               source[pos] != '"') {
            ++pos;
        }

        std::string result =
            source.substr(start, pos - start);

        if (pos >= source.size()) {
            throw std::runtime_error("unterminated string");
        }

        ++pos;

        return {
            TOK_STRING,
            result
        };
    }
};

/* ============================================================
 * Parser
 * ============================================================ */

class Parser {
private:
    Lexer lexer;
    Lexer::Token current;
    Lexer::Token next_token;

public:
    explicit Parser(std::string src)
        : lexer(std::move(src)) {
        current = lexer.next();
        next_token = lexer.next();
    }

    NodePtr parse() {
        auto block =
            std::make_shared<Node>(Node::NODE_BLOCK);

        while (current.type != Lexer::TOK_EOF) {
            block->children.push_back(statement());

            if (current.type == Lexer::TOK_SEMICOLON) {
                advance();
            }
        }

        return block;
    }

private:
    void advance() {
        current = next_token;
        next_token = lexer.next();
    }

    bool match(Lexer::TokenType t) {
        if (current.type == t) {
            advance();
            return true;
        }

        return false;
    }

    void expect(Lexer::TokenType t, const char* message) {
        if (!match(t)) {
            throw std::runtime_error(message);
        }
    }

    NodePtr statement() {
        if (current.type == Lexer::TOK_IDENT) {
            if (current.text == "if") return parse_if();
            if (current.text == "while") return parse_while();
            if (current.text == "for") return parse_for();
            if (current.text == "local") return parse_local();
            if (current.text == "return") return parse_return();
            if (current.text == "function") return parse_function_def();
        }
        return assignment();
    }

    NodePtr assignment() {
        auto left = logical_or();

        if (match(Lexer::TOK_COMMA)) {
            auto node = std::make_shared<Node>(Node::NODE_MULTI_ASSIGN);
            node->children.push_back(left);
            size_t lhs_count = 1;
            while (true) {
                auto lhs = logical_or();
                node->children.push_back(lhs);
                ++lhs_count;
                if (!match(Lexer::TOK_COMMA)) break;
            }
            node->text = std::to_string(lhs_count);
            expect(Lexer::TOK_EQUAL, "expected '='");
            node->children.push_back(logical_or());
            while (match(Lexer::TOK_COMMA)) {
                node->children.push_back(logical_or());
            }
            return node;
        }

        if (match(Lexer::TOK_EQUAL)) {
            auto node =
                std::make_shared<Node>(Node::NODE_ASSIGN);

            node->children.push_back(left);
            node->children.push_back(logical_or());

            return node;
        }

        return left;
    }

    NodePtr parse_block_until(const std::vector<std::string>& stop_words) {
        auto block = std::make_shared<Node>(Node::NODE_BLOCK);
        while (current.type != Lexer::TOK_EOF) {
            if (current.type == Lexer::TOK_IDENT) {
                for (size_t i = 0; i < stop_words.size(); ++i) {
                    if (current.text == stop_words[i]) {
                        return block;
                    }
                }
            }
            block->children.push_back(statement());
            if (current.type == Lexer::TOK_SEMICOLON) {
                advance();
            }
        }
        return block;
    }

    NodePtr parse_if() {
        advance();
        auto node = std::make_shared<Node>(Node::NODE_IF);
        node->children.push_back(logical_or());
        if (!(current.type == Lexer::TOK_IDENT && current.text == "then")) {
            throw std::runtime_error("expected 'then'");
        }
        advance();
        node->children.push_back(parse_block_until({"elseif", "else", "end"}));
        while (current.type == Lexer::TOK_IDENT && current.text == "elseif") {
            advance();
            node->children.push_back(logical_or());
            if (!(current.type == Lexer::TOK_IDENT && current.text == "then")) {
                throw std::runtime_error("expected 'then'");
            }
            advance();
            node->children.push_back(parse_block_until({"elseif", "else", "end"}));
        }
        if (current.type == Lexer::TOK_IDENT && current.text == "else") {
            advance();
            node->children.push_back(std::make_shared<Node>(Node::NODE_LITERAL));
            node->children.back()->literal = std::make_shared<Value>(true);
            node->children.push_back(parse_block_until({"end"}));
        }
        if (!(current.type == Lexer::TOK_IDENT && current.text == "end")) {
            throw std::runtime_error("expected 'end'");
        }
        advance();
        return node;
    }

    NodePtr parse_while() {
        advance();
        auto node = std::make_shared<Node>(Node::NODE_WHILE);
        node->children.push_back(logical_or());
        if (!(current.type == Lexer::TOK_IDENT && current.text == "do")) {
            throw std::runtime_error("expected 'do'");
        }
        advance();
        node->children.push_back(parse_block_until({"end"}));
        if (!(current.type == Lexer::TOK_IDENT && current.text == "end")) {
            throw std::runtime_error("expected 'end'");
        }
        advance();
        return node;
    }

    NodePtr parse_for() {
        advance();
        if (current.type != Lexer::TOK_IDENT) {
            throw std::runtime_error("expected loop variable");
        }
        auto node = std::make_shared<Node>(Node::NODE_FOR_NUMERIC);
        auto var = std::make_shared<Node>(Node::NODE_IDENT);
        var->text = current.text;
        const std::string first_name = current.text;
        node->children.push_back(var);
        advance();
        if (match(Lexer::TOK_COMMA)) {
            if (current.type != Lexer::TOK_IDENT) {
                throw std::runtime_error("expected second loop variable");
            }
            auto var2 = std::make_shared<Node>(Node::NODE_IDENT);
            var2->text = current.text;
            advance();
            if (!(current.type == Lexer::TOK_IDENT && current.text == "in")) {
                throw std::runtime_error("expected 'in'");
            }
            advance();
            auto generic = std::make_shared<Node>(Node::NODE_FOR_GENERIC);
            generic->children.push_back(var);
            generic->children.push_back(var2);
            generic->children.push_back(logical_or());
            while (match(Lexer::TOK_COMMA)) {
                generic->children.push_back(logical_or());
            }
            if (!(current.type == Lexer::TOK_IDENT && current.text == "do")) {
                throw std::runtime_error("expected 'do'");
            }
            advance();
            generic->children.push_back(parse_block_until({"end"}));
            if (!(current.type == Lexer::TOK_IDENT && current.text == "end")) {
                throw std::runtime_error("expected 'end'");
            }
            advance();
            return generic;
        }
        if (current.type == Lexer::TOK_IDENT && current.text == "in") {
            advance();
            auto generic = std::make_shared<Node>(Node::NODE_FOR_GENERIC);
            generic->children.push_back(var);
            auto nilvar = std::make_shared<Node>(Node::NODE_IDENT);
            nilvar->text = "";
            generic->children.push_back(nilvar);
            generic->children.push_back(logical_or());
            while (match(Lexer::TOK_COMMA)) {
                generic->children.push_back(logical_or());
            }
            if (!(current.type == Lexer::TOK_IDENT && current.text == "do")) {
                throw std::runtime_error("expected 'do'");
            }
            advance();
            generic->children.push_back(parse_block_until({"end"}));
            if (!(current.type == Lexer::TOK_IDENT && current.text == "end")) {
                throw std::runtime_error("expected 'end'");
            }
            advance();
            return generic;
        }
        expect(Lexer::TOK_EQUAL, "expected '='");
        node->children.push_back(logical_or());
        expect(Lexer::TOK_COMMA, "expected ','");
        node->children.push_back(logical_or());
        if (match(Lexer::TOK_COMMA)) {
            node->children.push_back(logical_or());
        } else {
            auto one = std::make_shared<Node>(Node::NODE_LITERAL);
            one->literal = std::make_shared<Value>(static_cast<int64_t>(1));
            node->children.push_back(one);
        }
        if (!(current.type == Lexer::TOK_IDENT && current.text == "do")) {
            throw std::runtime_error("expected 'do'");
        }
        advance();
        node->children.push_back(parse_block_until({"end"}));
        if (!(current.type == Lexer::TOK_IDENT && current.text == "end")) {
            throw std::runtime_error("expected 'end'");
        }
        advance();
        return node;
    }

    NodePtr parse_local() {
        advance();
        if (current.type != Lexer::TOK_IDENT) {
            throw std::runtime_error("expected local name");
        }
        auto node = std::make_shared<Node>(Node::NODE_LOCAL_ASSIGN);
        node->text = current.text;
        advance();
        if (match(Lexer::TOK_EQUAL)) {
            node->children.push_back(logical_or());
        }
        return node;
    }

    NodePtr parse_return() {
        advance();
        auto node = std::make_shared<Node>(Node::NODE_MULTI_RETURN);
        if (current.type != Lexer::TOK_SEMICOLON &&
            !(current.type == Lexer::TOK_IDENT &&
              (current.text == "end" || current.text == "elseif" || current.text == "else")) &&
            current.type != Lexer::TOK_EOF) {
            node->children.push_back(logical_or());
            while (match(Lexer::TOK_COMMA)) {
                node->children.push_back(logical_or());
            }
        }
        return node;
    }

    NodePtr parse_function_def() {
        advance();
        if (current.type != Lexer::TOK_IDENT) {
            throw std::runtime_error("expected function name");
        }
        auto node = std::make_shared<Node>(Node::NODE_FUNCTION_DEF);
        node->text = current.text;
        advance();
        expect(Lexer::TOK_LPAREN, "expected '('");
        auto params = std::make_shared<Node>(Node::NODE_BLOCK);
        if (current.type != Lexer::TOK_RPAREN) {
            while (true) {
                if (current.type != Lexer::TOK_IDENT) {
                    throw std::runtime_error("expected parameter name");
                }
                auto p = std::make_shared<Node>(Node::NODE_IDENT);
                p->text = current.text;
                params->children.push_back(p);
                advance();
                if (!match(Lexer::TOK_COMMA)) break;
            }
        }
        expect(Lexer::TOK_RPAREN, "expected ')'");
        node->children.push_back(params);
        node->children.push_back(parse_block_until({"end"}));
        if (!(current.type == Lexer::TOK_IDENT && current.text == "end")) {
            throw std::runtime_error("expected 'end'");
        }
        advance();
        return node;
    }

    NodePtr logical_or() {
        auto left = logical_and();
        while (current.type == Lexer::TOK_IDENT && current.text == "or") {
            advance();
            auto node = std::make_shared<Node>(Node::NODE_BINARY);
            node->text = "or";
            node->children.push_back(left);
            node->children.push_back(logical_and());
            left = node;
        }
        return left;
    }

    NodePtr logical_and() {
        auto left = equality();
        while (current.type == Lexer::TOK_IDENT && current.text == "and") {
            advance();
            auto node = std::make_shared<Node>(Node::NODE_BINARY);
            node->text = "and";
            node->children.push_back(left);
            node->children.push_back(equality());
            left = node;
        }
        return left;
    }

    NodePtr equality() {
        auto left = comparison();
        while (current.type == Lexer::TOK_EQ || current.type == Lexer::TOK_NE) {
            std::string op = current.text;
            advance();
            auto node = std::make_shared<Node>(Node::NODE_BINARY);
            node->text = op;
            node->children.push_back(left);
            node->children.push_back(comparison());
            left = node;
        }
        return left;
    }

    NodePtr comparison() {
        auto left = expression();
        while (current.type == Lexer::TOK_LT ||
               current.type == Lexer::TOK_LE ||
               current.type == Lexer::TOK_GT ||
               current.type == Lexer::TOK_GE) {
            std::string op = current.text;
            advance();
            auto node = std::make_shared<Node>(Node::NODE_BINARY);
            node->text = op;
            node->children.push_back(left);
            node->children.push_back(expression());
            left = node;
        }
        return left;
    }

    NodePtr expression() {
        auto left = term();

        while (current.type == Lexer::TOK_PLUS ||
               current.type == Lexer::TOK_MINUS) {

            std::string op = current.text;
            advance();

            auto node =
                std::make_shared<Node>(Node::NODE_BINARY);

            node->text = op;

            node->children.push_back(left);
            node->children.push_back(term());

            left = node;
        }

        return left;
    }

    NodePtr term() {
        auto left = unary();

        while (current.type == Lexer::TOK_STAR ||
               current.type == Lexer::TOK_SLASH) {

            std::string op = current.text;
            advance();

            auto node =
                std::make_shared<Node>(Node::NODE_BINARY);

            node->text = op;

            node->children.push_back(left);
            node->children.push_back(unary());

            left = node;
        }

        return left;
    }

    NodePtr unary() {
        if (match(Lexer::TOK_MINUS)) {
            auto node = std::make_shared<Node>(Node::NODE_UNARY);
            node->text = "-";
            node->children.push_back(unary());
            return node;
        }
        if (current.type == Lexer::TOK_IDENT && current.text == "not") {
            advance();
            auto node = std::make_shared<Node>(Node::NODE_UNARY);
            node->text = "not";
            node->children.push_back(unary());
            return node;
        }
        return postfix();
    }

    NodePtr postfix() {
        auto node = factor();

        while (true) {
            if (match(Lexer::TOK_LPAREN)) {
                auto call = std::make_shared<Node>(Node::NODE_CALL);
                call->children.push_back(node);

                if (current.type != Lexer::TOK_RPAREN) {
                    while (true) {
                        call->children.push_back(logical_or());
                        if (!match(Lexer::TOK_COMMA)) {
                            break;
                        }
                    }
                }

                expect(Lexer::TOK_RPAREN, "expected ')'");

                if (node->type == Node::NODE_IDENT) {
                    call->text = node->text;
                }
                node = call;
                continue;
            }

            if (current.type == Lexer::TOK_STRING ||
                current.type == Lexer::TOK_LBRACE) {
                auto call = std::make_shared<Node>(Node::NODE_CALL);
                call->children.push_back(node);
                call->children.push_back(factor());
                if (node->type == Node::NODE_IDENT) {
                    call->text = node->text;
                }
                node = call;
                continue;
            }

            if (match(Lexer::TOK_LBRACKET)) {
                auto index = std::make_shared<Node>(Node::NODE_INDEX);
                index->children.push_back(node);
                index->children.push_back(logical_or());
                expect(Lexer::TOK_RBRACKET, "expected ']'");
                node = index;
                continue;
            }

            if (match(Lexer::TOK_DOT)) {
                if (current.type != Lexer::TOK_IDENT) {
                    throw std::runtime_error("expected identifier after '.'");
                }

                auto key = std::make_shared<Node>(Node::NODE_LITERAL);
                key->literal = std::make_shared<Value>(current.text);
                advance();

                auto index = std::make_shared<Node>(Node::NODE_INDEX);
                index->children.push_back(node);
                index->children.push_back(key);
                node = index;
                continue;
            }

            if (match(Lexer::TOK_COLON)) {
                if (current.type != Lexer::TOK_IDENT) {
                    throw std::runtime_error("expected method name after ':'");
                }
                const std::string method_name = current.text;
                advance();

                auto key = std::make_shared<Node>(Node::NODE_LITERAL);
                key->literal = std::make_shared<Value>(method_name);

                auto callee = std::make_shared<Node>(Node::NODE_INDEX);
                callee->children.push_back(node);
                callee->children.push_back(key);

                auto call = std::make_shared<Node>(Node::NODE_CALL);
                call->children.push_back(callee);
                call->children.push_back(node);
                call->text = method_name;

                if (match(Lexer::TOK_LPAREN)) {
                    if (current.type != Lexer::TOK_RPAREN) {
                        while (true) {
                            call->children.push_back(logical_or());
                            if (!match(Lexer::TOK_COMMA)) {
                                break;
                            }
                        }
                    }
                    expect(Lexer::TOK_RPAREN, "expected ')'");
                } else if (current.type == Lexer::TOK_STRING ||
                           current.type == Lexer::TOK_LBRACE) {
                    call->children.push_back(factor());
                } else {
                    throw std::runtime_error("expected method call arguments");
                }
                node = call;
                continue;
            }

            break;
        }

        return node;
    }

    NodePtr factor() {
        switch (current.type) {
            case Lexer::TOK_INT: {
                auto node =
                    std::make_shared<Node>(Node::NODE_LITERAL);

                node->literal =
                    std::make_shared<Value>(
                        static_cast<int64_t>(
                            std::stoll(current.text)
                        )
                    );

                advance();
                return node;
            }

            case Lexer::TOK_FLOAT: {
                auto node =
                    std::make_shared<Node>(Node::NODE_LITERAL);

                node->literal =
                    std::make_shared<Value>(
                        std::stod(current.text)
                    );

                advance();
                return node;
            }

            case Lexer::TOK_STRING: {
                auto node =
                    std::make_shared<Node>(Node::NODE_LITERAL);

                node->literal =
                    std::make_shared<Value>(current.text);

                advance();
                return node;
            }

            case Lexer::TOK_IDENT: {
                if (current.text == "function") {
                    advance();
                    auto node = std::make_shared<Node>(Node::NODE_FUNCTION_EXPR);
                    expect(Lexer::TOK_LPAREN, "expected '('");
                    auto params = std::make_shared<Node>(Node::NODE_BLOCK);
                    if (current.type != Lexer::TOK_RPAREN) {
                        while (true) {
                            if (current.type != Lexer::TOK_IDENT) {
                                throw std::runtime_error("expected parameter name");
                            }
                            auto p = std::make_shared<Node>(Node::NODE_IDENT);
                            p->text = current.text;
                            params->children.push_back(p);
                            advance();
                            if (!match(Lexer::TOK_COMMA)) break;
                        }
                    }
                    expect(Lexer::TOK_RPAREN, "expected ')'");
                    node->children.push_back(params);
                    node->children.push_back(parse_block_until({"end"}));
                    if (!(current.type == Lexer::TOK_IDENT && current.text == "end")) {
                        throw std::runtime_error("expected 'end'");
                    }
                    advance();
                    return node;
                }
                if (current.text == "true") {
                    auto node = std::make_shared<Node>(Node::NODE_LITERAL);
                    node->literal = std::make_shared<Value>(true);
                    advance();
                    return node;
                }
                if (current.text == "false") {
                    auto node = std::make_shared<Node>(Node::NODE_LITERAL);
                    node->literal = std::make_shared<Value>(false);
                    advance();
                    return node;
                }
                if (current.text == "nil") {
                    auto node = std::make_shared<Node>(Node::NODE_LITERAL);
                    node->literal = std::make_shared<Value>();
                    advance();
                    return node;
                }
                auto node =
                    std::make_shared<Node>(Node::NODE_IDENT);

                node->text = current.text;
                advance();

                return node;
            }

            case Lexer::TOK_LPAREN: {
                advance();

                auto node = logical_or();

                if (!match(Lexer::TOK_RPAREN)) {
                    throw std::runtime_error("expected ')'");
                }

                return node;
            }

            case Lexer::TOK_LBRACE:
                return table_constructor();

            default:
                throw std::runtime_error("unexpected token");
        }
    }

    NodePtr table_constructor() {
        expect(Lexer::TOK_LBRACE, "expected '{'");

        auto table = std::make_shared<Node>(Node::NODE_TABLE);
        int64_t array_index = 1;

        while (current.type != Lexer::TOK_RBRACE) {
            NodePtr key;
            NodePtr value;

            if (match(Lexer::TOK_LBRACKET)) {
                key = logical_or();
                expect(Lexer::TOK_RBRACKET, "expected ']'");
                expect(Lexer::TOK_EQUAL, "expected '='");
                value = logical_or();
            } else if (current.type == Lexer::TOK_IDENT &&
                       next_token.type == Lexer::TOK_EQUAL) {
                key = std::make_shared<Node>(Node::NODE_LITERAL);
                key->literal = std::make_shared<Value>(current.text);
                advance();
                expect(Lexer::TOK_EQUAL, "expected '='");
                value = logical_or();
            } else {
                key = std::make_shared<Node>(Node::NODE_LITERAL);
                key->literal = std::make_shared<Value>(array_index++);
                value = logical_or();
            }

            table->children.push_back(key);
            table->children.push_back(value);

            if (!match(Lexer::TOK_COMMA) && !match(Lexer::TOK_SEMICOLON)) {
                break;
            }
        }

        expect(Lexer::TOK_RBRACE, "expected '}'");
        return table;
    }
};

/* ============================================================
 * Context / Evaluator
 * ============================================================ */

class Context {
public:
    std::map<std::string, ValuePtr> globals;
    std::vector<std::map<std::string, ValuePtr>> scopes;
    void* c_userdata = nullptr;
    qamrpp_error_code last_error_code = QAMRPP_OK;
    std::string last_error_message;

    std::vector<std::unique_ptr<Extension>> extensions;

    struct LoadedPlugin {
        void* handle = nullptr;
        const qamrpp_plugin_descriptor* descriptor = nullptr;
    };

    std::vector<LoadedPlugin> plugins;
    Linker linker;
    std::map<HookPoint, std::vector<std::function<void(Context&, const HookPayload&)>>> hooks;
    qamrpp_host_api host_api_cache = {};
    bool return_flag = false;
    ValuePtr return_value;
    std::vector<ValuePtr> return_values;
    std::vector<ValuePtr> last_call_returns;

public:
    Context() {
        host_api_cache = make_host_api();
        scopes.push_back(std::map<std::string, ValuePtr>());
        install_builtins();
    }

    ~Context() {
        unload_plugins();
    }

    void register_native(
        const std::string& name,
        NativeFn fn
    ) {
        scopes.back()[name] =
            std::make_shared<Value>(std::move(fn));
    }

    void push_scope() { scopes.push_back(std::map<std::string, ValuePtr>()); }
    void pop_scope() { if (scopes.size() > 1) scopes.pop_back(); }

    ValuePtr lookup_name(const std::string& name) const {
        for (size_t i = scopes.size(); i > 0; --i) {
            auto it = scopes[i - 1].find(name);
            if (it != scopes[i - 1].end()) return it->second;
        }
        auto it = globals.find(name);
        if (it != globals.end()) return it->second;
        return nullptr;
    }

    void assign_name(const std::string& name, const ValuePtr& value) {
        for (size_t i = scopes.size(); i > 0; --i) {
            auto it = scopes[i - 1].find(name);
            if (it != scopes[i - 1].end()) {
                it->second = value;
                return;
            }
        }
        scopes.front()[name] = value;
    }

    void define_local(const std::string& name, const ValuePtr& value) {
        scopes.back()[name] = value;
    }

    void add_extension(std::unique_ptr<Extension> ext) {
        ext->register_functions(*this);
        extensions.push_back(std::move(ext));
    }

    void add_hook(HookPoint point, std::function<void(Context&, const HookPayload&)> callback) {
        hooks[point].push_back(std::move(callback));
    }

    void emit_hook(HookPoint point, const HookPayload& payload) {
        auto it = hooks.find(point);
        if (it == hooks.end()) {
            return;
        }
        for (size_t i = 0; i < it->second.size(); ++i) {
            it->second[i](*this, payload);
        }
    }

    ValuePtr make_table() const {
        return Value::make_table();
    }

    ValuePtr table_raw_lookup(const ValuePtr& table, const ValuePtr& key) const {
        if (!table || table->type != Value::TABLE) {
            throw std::runtime_error("raw table access expects a table");
        }
        if (!key || key->type == Value::NIL) {
            throw std::runtime_error("table key must not be nil");
        }
        const auto& entries = table->table_entries;
        for (size_t i = 0; i < entries.size(); ++i) {
            if (table_key_equals(entries[i].first, key)) {
                return entries[i].second;
            }
        }
        return nullptr;
    }

    ValuePtr table_raw_get(const ValuePtr& table, const ValuePtr& key) const {
        ValuePtr value = table_raw_lookup(table, key);
        if (value) {
            return value;
        }
        return std::make_shared<Value>();
    }

    void table_raw_set(const ValuePtr& table, const ValuePtr& key, const ValuePtr& value) {
        if (!table || table->type != Value::TABLE) {
            throw std::runtime_error("raw table write expects a table");
        }
        if (!key || key->type == Value::NIL) {
            throw std::runtime_error("table key must not be nil");
        }
        auto& entries = table->table_entries;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (!table_key_equals(it->first, key)) {
                continue;
            }
            if (!value || value->type == Value::NIL) {
                entries.erase(it);
            } else {
                it->second = value;
            }
            return;
        }
        if (value && value->type != Value::NIL) {
            entries.push_back(std::make_pair(key, value));
        }
    }

    ValuePtr table_get(const ValuePtr& table, const ValuePtr& key) {
        ValuePtr raw = table_raw_lookup(table, key);
        if (raw) {
            return raw;
        }
        ValuePtr index_metamethod = lookup_metamethod(table, "__index");
        if (!index_metamethod) {
            return std::make_shared<Value>();
        }
        if (index_metamethod->type == Value::FUNCTION) {
            std::vector<ValuePtr> args;
            args.push_back(table);
            args.push_back(key);
            return index_metamethod->function_value(*this, args);
        }
        if (index_metamethod->type == Value::TABLE) {
            return table_get(index_metamethod, key);
        }
        throw std::runtime_error("__index metamethod must be function or table");
    }

    void table_set(const ValuePtr& table, const ValuePtr& key, const ValuePtr& value) {
        ValuePtr raw = table_raw_lookup(table, key);
        if (raw) {
            table_raw_set(table, key, value);
            return;
        }
        ValuePtr newindex_metamethod = lookup_metamethod(table, "__newindex");
        if (!newindex_metamethod) {
            table_raw_set(table, key, value);
            return;
        }
        if (newindex_metamethod->type == Value::FUNCTION) {
            std::vector<ValuePtr> args;
            args.push_back(table);
            args.push_back(key);
            args.push_back(value ? value : std::make_shared<Value>());
            (void)newindex_metamethod->function_value(*this, args);
            return;
        }
        if (newindex_metamethod->type == Value::TABLE) {
            table_set(newindex_metamethod, key, value);
            return;
        }
        throw std::runtime_error("__newindex metamethod must be function or table");
    }

    ValuePtr get_metatable(const ValuePtr& value) const {
        if (!value || !value->metatable_value) {
            return nullptr;
        }
        return value->metatable_value;
    }

    void set_metatable(const ValuePtr& value, const ValuePtr& metatable) {
        if (!value) {
            throw std::runtime_error("setmetatable expects a value");
        }
        if (value->type != Value::TABLE && value->type != Value::USERDATA) {
            throw std::runtime_error("metatables are only supported for table/userdata values");
        }
        if (metatable && metatable->type != Value::NIL && metatable->type != Value::TABLE) {
            throw std::runtime_error("metatable must be nil or table");
        }
        if (!metatable || metatable->type == Value::NIL) {
            value->metatable_value.reset();
            return;
        }
        value->metatable_value = metatable;
    }

    bool load_library(const std::string& path) {
#ifdef _WIN32
        HMODULE lib =
            LoadLibraryA(path.c_str());

        if (!lib) {
            return false;
        }

        auto* descriptor =
            reinterpret_cast<const qamrpp_plugin_descriptor*>(
                GetProcAddress(
                    lib,
                    "qamrpp_plugin_descriptor"
                )
            );
        using get_lib_desc_fn = const qamrpp_library_descriptor* (*)();
        get_lib_desc_fn get_lib_descriptor =
            reinterpret_cast<get_lib_desc_fn>(
                GetProcAddress(lib, "qamrpp_get_library_descriptor")
                
            );
        using set_api_fn = void (*)(const qamrpp_host_api*);
        set_api_fn set_host_api =
            reinterpret_cast<set_api_fn>(
                GetProcAddress(lib, "qamrpp_library_set_host_api")
            );

#else
        void* lib =
            dlopen(path.c_str(), RTLD_NOW);

        if (!lib) {
            return false;
        }

        auto* descriptor =
            reinterpret_cast<const qamrpp_plugin_descriptor*>(
                dlsym(
                    lib,
                    "qamrpp_plugin_descriptor"
                )
            );
        typedef const qamrpp_library_descriptor* (*get_lib_desc_fn)();
        get_lib_desc_fn get_lib_descriptor =
            reinterpret_cast<get_lib_desc_fn>(
                dlsym(lib, "qamrpp_get_library_descriptor")
            );
        typedef void (*set_api_fn)(const qamrpp_host_api*);
        set_api_fn set_host_api =
            reinterpret_cast<set_api_fn>(
                dlsym(lib, "qamrpp_library_set_host_api")
            );
#endif

        if (set_host_api) {
            set_host_api(&host_api_cache);
        }

        if (!descriptor && !get_lib_descriptor) {
#ifdef _WIN32
            FreeLibrary(lib);
#else
            dlclose(lib);
#endif
            return false;
        }

        if (get_lib_descriptor) {
            const qamrpp_library_descriptor* c_desc = get_lib_descriptor();
            if (!c_desc || c_desc->api_version != QAMRPP_LIBRARY_API_VERSION) {
#ifdef _WIN32
                FreeLibrary(lib);
#else
                dlclose(lib);
#endif
                return false;
            }
            for (size_t i = 0; i < c_desc->function_count; ++i) {
                const qamrpp_native_binding& fn = c_desc->functions[i];
                if (!fn.name || !fn.function) continue;
                register_native(
                    fn.name,
                    [native = fn.function](Context& ctx, std::vector<ValuePtr>& args) -> ValuePtr {
                        std::vector<qamrpp_value*> c_args;
                        for (size_t j = 0; j < args.size(); ++j) {
                            c_args.push_back(reinterpret_cast<qamrpp_value*>(args[j].get()));
                        }
                        qamrpp_value* result = native(reinterpret_cast<qamrpp_context*>(&ctx), c_args.data(), c_args.size());
                        return result ? ValuePtr(reinterpret_cast<Value*>(result)) : std::make_shared<Value>();
                    }
                );
            }
            if (c_desc->on_load && c_desc->on_load(reinterpret_cast<qamrpp_context*>(this), &host_api_cache) != 0) {
#ifdef _WIN32
                FreeLibrary(lib);
#else
                dlclose(lib);
#endif
                return false;
            }
            return true;
        }

        if (descriptor->api_version !=
            QAMRPP_PLUGIN_API_VERSION) {
#ifdef _WIN32
            FreeLibrary(lib);
#else
            dlclose(lib);
#endif
            return false;
        }

        if (!descriptor->functions && descriptor->function_count != 0) {
#ifdef _WIN32
            FreeLibrary(lib);
#else
            dlclose(lib);
#endif
            return false;
        }

        for (size_t i = 0;
             i < descriptor->function_count;
             ++i) {

            auto& fn = descriptor->functions[i];

            if (!fn.name || !fn.function) {
                continue;
            }

            register_native(
                fn.name,
                [native = fn.function](
                    Context& ctx,
                    std::vector<ValuePtr>& args
                ) -> ValuePtr {

                    std::vector<qamrpp_value*> c_args;

                    for (auto& a : args) {
                        c_args.push_back(
                            reinterpret_cast<qamrpp_value*>(
                                a.get()
                            )
                        );
                    }

                    auto* result =
                        native(
                            reinterpret_cast<qamrpp_context*>(&ctx),
                            c_args.data(),
                            c_args.size()
                        );

                    return ValuePtr(
                        reinterpret_cast<Value*>(result)
                    );
                }
            );
        }

        if (descriptor->on_load) {
            if (descriptor->on_load(
                reinterpret_cast<qamrpp_context*>(this)
            ) != 0) {
#ifdef _WIN32
                FreeLibrary(lib);
#else
                dlclose(lib);
#endif
                return false;
            }
        }

        LoadedPlugin loaded;
        loaded.handle = lib;
        loaded.descriptor = descriptor;
        plugins.push_back(loaded);

        return true;
    }

    bool load_library_named(const std::string& name) {
        const std::vector<std::string> roots = library_search_roots();
        for (size_t i = 0; i < roots.size(); ++i) {
            const std::string candidate = roots[i] + "/" + library_file_name(name);
            if (load_library(candidate)) {
                return true;
            }
        }
        return false;
    }

    ValuePtr eval(const NodePtr& node) {
        switch (node->type) {
            case Node::NODE_LITERAL:
                return node->literal;

            case Node::NODE_IDENT: {
                auto value = lookup_name(node->text);
                if (!value) {
                    throw std::runtime_error(
                        "undefined variable: " + node->text
                    );
                }
                return value;
            }

            case Node::NODE_ASSIGN: {
                auto value =
                    eval(node->children[1]);

                auto target = node->children[0];
                if (target->type == Node::NODE_IDENT) {
                    assign_name(target->text, value);
                    return value;
                }

                if (target->type == Node::NODE_INDEX) {
                    if (target->children.size() != 2) {
                        throw std::runtime_error("invalid table assignment target");
                    }
                    auto table = eval(target->children[0]);
                    auto key = eval(target->children[1]);
                    table_set(table, key, value);
                    return value;
                }

                throw std::runtime_error(
                    "invalid assignment target"
                );
            }
            case Node::NODE_MULTI_ASSIGN: {
                const size_t eq_pos = static_cast<size_t>(std::stoul(node->text));
                std::vector<NodePtr> lhs(node->children.begin(), node->children.begin() + static_cast<long>(eq_pos));
                std::vector<NodePtr> rhs_nodes(node->children.begin() + static_cast<long>(eq_pos), node->children.end());
                std::vector<ValuePtr> rhs = eval_expr_list(rhs_nodes);
                if (rhs.size() < lhs.size()) rhs.resize(lhs.size(), std::make_shared<Value>());
                for (size_t i = 0; i < lhs.size(); ++i) {
                    auto target = lhs[i];
                    auto value = i < rhs.size() ? rhs[i] : std::make_shared<Value>();
                    if (target->type == Node::NODE_IDENT) {
                        assign_name(target->text, value);
                    } else if (target->type == Node::NODE_INDEX) {
                        auto table = eval(target->children[0]);
                        auto key = eval(target->children[1]);
                        table_set(table, key, value);
                    } else {
                        throw std::runtime_error("invalid assignment target");
                    }
                }
                return rhs.empty() ? std::make_shared<Value>() : rhs[0];
            }

            case Node::NODE_TABLE: {
                auto table = make_table();
                for (size_t i = 0; i + 1 < node->children.size(); i += 2) {
                    ValuePtr key = eval(node->children[i]);
                    ValuePtr value = eval(node->children[i + 1]);
                    table_raw_set(table, key, value);
                }
                return table;
            }

            case Node::NODE_INDEX: {
                if (node->children.size() != 2) {
                    throw std::runtime_error("invalid table index expression");
                }
                auto table = eval(node->children[0]);
                auto key = eval(node->children[1]);
                return table_get(table, key);
            }

            case Node::NODE_LOCAL_ASSIGN: {
                ValuePtr value = std::make_shared<Value>();
                if (!node->children.empty()) value = eval(node->children[0]);
                define_local(node->text, value);
                return value;
            }

            case Node::NODE_RETURN:
            case Node::NODE_MULTI_RETURN: {
                return_flag = true;
                if (node->children.empty()) {
                    return_value = std::make_shared<Value>();
                    return_values.clear();
                } else {
                    return_values = eval_expr_list(node->children);
                    return_value = return_values.empty() ? std::make_shared<Value>() : return_values[0];
                }
                return return_value;
            }

            case Node::NODE_IF: {
                for (size_t i = 0; i + 1 < node->children.size(); i += 2) {
                    if (eval(node->children[i])->truthy()) {
                        push_scope();
                        ValuePtr v = eval(node->children[i + 1]);
                        pop_scope();
                        return v;
                    }
                }
                return std::make_shared<Value>();
            }

            case Node::NODE_WHILE: {
                ValuePtr out = std::make_shared<Value>();
                while (eval(node->children[0])->truthy()) {
                    push_scope();
                    out = eval(node->children[1]);
                    pop_scope();
                    if (return_flag) return out;
                }
                return out;
            }

            case Node::NODE_FOR_NUMERIC: {
                ValuePtr start_v = eval(node->children[1]);
                ValuePtr end_v = eval(node->children[2]);
                ValuePtr step_v = eval(node->children[3]);
                const double start = start_v->type == Value::FLOAT ? start_v->float_value : static_cast<double>(start_v->int_value);
                const double finish = end_v->type == Value::FLOAT ? end_v->float_value : static_cast<double>(end_v->int_value);
                const double step = step_v->type == Value::FLOAT ? step_v->float_value : static_cast<double>(step_v->int_value);
                if (step == 0.0) throw std::runtime_error("for step cannot be zero");
                ValuePtr out = std::make_shared<Value>();
                for (double i = start; step > 0 ? i <= finish : i >= finish; i += step) {
                    push_scope();
                    define_local(node->children[0]->text, std::make_shared<Value>(i));
                    out = eval(node->children[4]);
                    pop_scope();
                    if (return_flag) return out;
                }
                return out;
            }
            case Node::NODE_FOR_GENERIC: {
                const std::string key_name = node->children[0]->text;
                const std::string value_name = node->children[1]->text;
                std::vector<NodePtr> iter_nodes(node->children.begin() + 2, node->children.end() - 1);
                std::vector<ValuePtr> iter_values = eval_expr_list(iter_nodes);
                if (iter_values.empty()) throw std::runtime_error("generic for expects iterator");
                ValuePtr iterator = iter_values[0];
                ValuePtr state = iter_values.size() > 1 ? iter_values[1] : std::make_shared<Value>();
                ValuePtr control = iter_values.size() > 2 ? iter_values[2] : std::make_shared<Value>();
                if (!iterator || iterator->type != Value::FUNCTION) {
                    throw std::runtime_error("generic for iterator must be a function");
                }
                ValuePtr out = std::make_shared<Value>();
                while (true) {
                    std::vector<ValuePtr> iargs;
                    iargs.push_back(state);
                    iargs.push_back(control);
                    ValuePtr first = iterator->function_value(*this, iargs);
                    std::vector<ValuePtr> produced = last_call_returns.empty()
                        ? std::vector<ValuePtr>(1, first)
                        : last_call_returns;
                    if (produced.empty() || !produced[0] || produced[0]->type == Value::NIL) {
                        break;
                    }
                    control = produced[0];
                    push_scope();
                    define_local(key_name, produced[0]);
                    if (!value_name.empty()) {
                        define_local(value_name, produced.size() > 1 ? produced[1] : std::make_shared<Value>());
                    }
                    out = eval(node->children.back());
                    pop_scope();
                    if (return_flag) return out;
                }
                return out;
            }

            case Node::NODE_FUNCTION_DEF: {
                std::vector<std::string> params;
                for (size_t i = 0; i < node->children[0]->children.size(); ++i) {
                    params.push_back(node->children[0]->children[i]->text);
                }
                NodePtr body = node->children[1];
                NativeFn fn = [params, body](Context& call_ctx, std::vector<ValuePtr>& args) -> ValuePtr {
                    call_ctx.push_scope();
                    for (size_t i = 0; i < params.size(); ++i) {
                        ValuePtr a = i < args.size() ? args[i] : std::make_shared<Value>();
                        call_ctx.define_local(params[i], a);
                    }
                    const bool prev_return = call_ctx.return_flag;
                    ValuePtr prev_value = call_ctx.return_value;
                    std::vector<ValuePtr> prev_values = call_ctx.return_values;
                    call_ctx.return_flag = false;
                    call_ctx.return_value = std::make_shared<Value>();
                    call_ctx.return_values.clear();
                    ValuePtr out = call_ctx.eval(body);
                    ValuePtr ret = call_ctx.return_flag ? call_ctx.return_value : out;
                    if (!call_ctx.return_values.empty()) {
                        call_ctx.last_call_returns = call_ctx.return_values;
                    } else {
                        call_ctx.last_call_returns.assign(1, ret);
                    }
                    call_ctx.return_flag = prev_return;
                    call_ctx.return_value = prev_value;
                    call_ctx.return_values = prev_values;
                    call_ctx.pop_scope();
                    return ret;
                };
                ValuePtr fn_value = std::make_shared<Value>(fn);
                assign_name(node->text, fn_value);
                return fn_value;
            }
            case Node::NODE_FUNCTION_EXPR: {
                std::vector<std::string> params;
                for (size_t i = 0; i < node->children[0]->children.size(); ++i) {
                    params.push_back(node->children[0]->children[i]->text);
                }
                NodePtr body = node->children[1];
                NativeFn fn = [params, body](Context& call_ctx, std::vector<ValuePtr>& args) -> ValuePtr {
                    call_ctx.push_scope();
                    for (size_t i = 0; i < params.size(); ++i) {
                        ValuePtr a = i < args.size() ? args[i] : std::make_shared<Value>();
                        call_ctx.define_local(params[i], a);
                    }
                    const bool prev_return = call_ctx.return_flag;
                    ValuePtr prev_value = call_ctx.return_value;
                    std::vector<ValuePtr> prev_values = call_ctx.return_values;
                    call_ctx.return_flag = false;
                    call_ctx.return_value = std::make_shared<Value>();
                    call_ctx.return_values.clear();
                    ValuePtr out = call_ctx.eval(body);
                    ValuePtr ret = call_ctx.return_flag ? call_ctx.return_value : out;
                    if (!call_ctx.return_values.empty()) {
                        call_ctx.last_call_returns = call_ctx.return_values;
                    } else {
                        call_ctx.last_call_returns.assign(1, ret);
                    }
                    call_ctx.return_flag = prev_return;
                    call_ctx.return_value = prev_value;
                    call_ctx.return_values = prev_values;
                    call_ctx.pop_scope();
                    return ret;
                };
                return std::make_shared<Value>(fn);
            }

            case Node::NODE_BINARY:
                return eval_binary(node);
            case Node::NODE_UNARY:
                return eval_unary(node);

            case Node::NODE_CALL:
                return eval_call(node);

            case Node::NODE_BLOCK: {
                ValuePtr result =
                    std::make_shared<Value>();

                for (auto& child : node->children) {
                    result = eval(child);
                    if (return_flag) return result;
                }

                return result;
            }
        }

        return std::make_shared<Value>();
    }

    ValuePtr run(const std::string& source) {
        HookPayload before;
        before.source = &source;
        emit_hook(HookPoint::BeforeRun, before);

        try {
            Parser parser(source);
            auto ast = parser.parse();
            HookPayload parsed;
            parsed.source = &source;
            parsed.ast = ast.get();
            emit_hook(HookPoint::AfterParse, parsed);

            ValuePtr result = eval(ast);
            HookPayload after;
            after.source = &source;
            emit_hook(HookPoint::AfterEval, after);
            return result;
        } catch (const std::exception& e) {
            HookPayload on_error;
            on_error.source = &source;
            on_error.error_message = e.what();
            emit_hook(HookPoint::OnError, on_error);
            throw;
        }
    }

    void load_standard_library(StdLib libs = StdLib::ALL) {
        if ((libs & StdLib::CORE) != StdLib::NONE) (void)load_library_named("core");
        if ((libs & StdLib::STRING) != StdLib::NONE) (void)load_library_named("string");
        if ((libs & StdLib::TABLE) != StdLib::NONE) (void)load_library_named("table");
        if ((libs & StdLib::MATH) != StdLib::NONE) (void)load_library_named("math");
        if ((libs & StdLib::IO) != StdLib::NONE) (void)load_library_named("io");
        if ((libs & StdLib::OS) != StdLib::NONE) (void)load_library_named("os");
        if ((libs & StdLib::DEBUGLIB) != StdLib::NONE) (void)load_library_named("debug");
        if ((libs & StdLib::COROUTINE) != StdLib::NONE) (void)load_library_named("coroutine");
        if ((libs & StdLib::PACKAGE) != StdLib::NONE) (void)load_library_named("package");
        if ((libs & StdLib::UTF8) != StdLib::NONE) (void)load_library_named("utf8");
    }

private:
    static qamrpp_host_api make_host_api() {
        qamrpp_host_api api = {};
        api.api_version = QAMRPP_LIBRARY_API_VERSION;
        api.value_nil = &qamrpp_value_nil;
        api.value_bool = &qamrpp_value_bool;
        api.value_int = &qamrpp_value_int;
        api.value_float = &qamrpp_value_float;
        api.value_string = &qamrpp_value_string;
        api.value_userdata = &qamrpp_value_userdata;
        api.value_table = &qamrpp_value_table;
        api.value_get_type = &qamrpp_value_get_type;
        api.value_as_bool = &qamrpp_value_as_bool;
        api.value_as_int = &qamrpp_value_as_int;
        api.value_as_float = &qamrpp_value_as_float;
        api.value_as_string = &qamrpp_value_as_string;
        api.value_as_userdata = &qamrpp_value_as_userdata;
        api.table_raw_get = &qamrpp_table_raw_get;
        api.table_raw_set = &qamrpp_table_raw_set;
        api.table_get = &qamrpp_table_get;
        api.table_set = &qamrpp_table_set;
        api.value_get_metatable = &qamrpp_value_get_metatable;
        api.value_set_metatable = &qamrpp_value_set_metatable;
        api.set_error = &qamrpp_set_error;
        api.context_get_userdata = &qamrpp_context_get_userdata;
        api.context_set_userdata = &qamrpp_context_set_userdata;
        api.get_global = &qamrpp_get_global;
        api.set_global = &qamrpp_set_global;
        return api;
    }

    static std::string library_file_name(const std::string& name) {
#ifdef _WIN32
        return name + ".dll";
#elif __APPLE__
        return "libqamrpp_" + name + ".dylib";
#else
        return "libqamrpp_" + name + ".so";
#endif
    }

    static std::vector<std::string> library_search_roots() {
        std::vector<std::string> roots;
        const char* env = std::getenv("QAMRPP_PATH");
        if (env && *env) {
            std::string path = env;
            size_t start = 0;
            while (start <= path.size()) {
                size_t end = path.find(':', start);
                std::string part = path.substr(start, end - start);
                if (!part.empty()) roots.push_back(part);
                if (end == std::string::npos) break;
                start = end + 1;
            }
        }
        const char* home = std::getenv("HOME");
        if (home && *home) {
            roots.push_back(std::string(home) + "/.qamrpp");
        }
        roots.push_back(".");
        return roots;
    }

    static bool table_key_equals(const ValuePtr& lhs, const ValuePtr& rhs) {
        if (lhs.get() == rhs.get()) {
            return true;
        }
        if (!lhs || !rhs) {
            return false;
        }
        if ((lhs->type == Value::INT || lhs->type == Value::FLOAT) &&
            (rhs->type == Value::INT || rhs->type == Value::FLOAT)) {
            const double l = lhs->type == Value::INT
                ? static_cast<double>(lhs->int_value)
                : lhs->float_value;
            const double r = rhs->type == Value::INT
                ? static_cast<double>(rhs->int_value)
                : rhs->float_value;
            return l == r;
        }
        if (lhs->type != rhs->type) {
            return false;
        }
        switch (lhs->type) {
            case Value::NIL:
                return true;
            case Value::BOOL:
                return lhs->bool_value == rhs->bool_value;
            case Value::INT:
                return lhs->int_value == rhs->int_value;
            case Value::FLOAT:
                return lhs->float_value == rhs->float_value;
            case Value::STRING:
                return lhs->string_value == rhs->string_value;
            case Value::USERDATA:
                return lhs->userdata_value == rhs->userdata_value;
            case Value::TABLE:
                return false;
            default:
                return false;
        }
    }

    ValuePtr lookup_metamethod(const ValuePtr& value, const char* name) const {
        if (!value || !value->metatable_value || value->metatable_value->type != Value::TABLE) {
            return nullptr;
        }
        ValuePtr key = std::make_shared<Value>(std::string(name));
        ValuePtr metamethod = table_raw_lookup(value->metatable_value, key);
        if (!metamethod || metamethod->type == Value::NIL) {
            return nullptr;
        }
        return metamethod;
    }

    ValuePtr invoke_binary_metamethod(
        const ValuePtr& lhs,
        const ValuePtr& rhs,
        const char* metamethod_name
    ) {
        ValuePtr metamethod = lookup_metamethod(lhs, metamethod_name);
        if (!metamethod) {
            metamethod = lookup_metamethod(rhs, metamethod_name);
        }
        if (!metamethod) {
            return nullptr;
        }
        if (metamethod->type != Value::FUNCTION) {
            throw std::runtime_error(std::string(metamethod_name) + " metamethod is not callable");
        }
        std::vector<ValuePtr> args;
        args.push_back(lhs);
        args.push_back(rhs);
        return metamethod->function_value(*this, args);
    }

    std::string value_to_string(const ValuePtr& value) {
        if (!value) {
            return "nil";
        }
        ValuePtr tostring_metamethod = lookup_metamethod(value, "__tostring");
        if (tostring_metamethod) {
            if (tostring_metamethod->type != Value::FUNCTION) {
                throw std::runtime_error("__tostring metamethod is not callable");
            }
            std::vector<ValuePtr> args;
            args.push_back(value);
            ValuePtr result = tostring_metamethod->function_value(*this, args);
            if (!result || result->type == Value::NIL) {
                return "nil";
            }
            return result->to_string();
        }
        return value->to_string();
    }

    void install_builtins() {
        register_native(
            "print",
            [](Context& ctx,
               std::vector<ValuePtr>& args) -> ValuePtr {

                for (size_t i = 0; i < args.size(); ++i) {
                    if (i != 0) {
                        std::printf(" ");
                    }

                    std::printf(
                        "%s",
                        ctx.value_to_string(args[i]).c_str()
                    );
                }

                std::printf("\n");

                return std::make_shared<Value>();
            }
        );

        register_native(
            "str",
            [](Context& ctx,
               std::vector<ValuePtr>& args) -> ValuePtr {

                if (args.empty()) {
                    return std::make_shared<Value>(
                        std::string("")
                    );
                }

                return std::make_shared<Value>(
                    ctx.value_to_string(args[0])
                );
            }
        );

        register_native(
            "table_new",
            [](Context& ctx,
               std::vector<ValuePtr>&) -> ValuePtr {
                return ctx.make_table();
            }
        );

        register_native(
            "table_rawget",
            [](Context& ctx,
               std::vector<ValuePtr>& args) -> ValuePtr {
                if (args.size() < 2) {
                    throw std::runtime_error("table_rawget expects table and key");
                }
                return ctx.table_raw_get(args[0], args[1]);
            }
        );

        register_native(
            "table_rawset",
            [](Context& ctx,
               std::vector<ValuePtr>& args) -> ValuePtr {
                if (args.size() < 3) {
                    throw std::runtime_error("table_rawset expects table, key, and value");
                }
                ctx.table_raw_set(args[0], args[1], args[2]);
                return args[0];
            }
        );

        register_native(
            "table_get",
            [](Context& ctx,
               std::vector<ValuePtr>& args) -> ValuePtr {
                if (args.size() < 2) {
                    throw std::runtime_error("table_get expects table and key");
                }
                return ctx.table_get(args[0], args[1]);
            }
        );

        register_native(
            "table_set",
            [](Context& ctx,
               std::vector<ValuePtr>& args) -> ValuePtr {
                if (args.size() < 3) {
                    throw std::runtime_error("table_set expects table, key, and value");
                }
                ctx.table_set(args[0], args[1], args[2]);
                return args[0];
            }
        );

        register_native(
            "getmetatable",
            [](Context& ctx,
               std::vector<ValuePtr>& args) -> ValuePtr {
                if (args.empty()) {
                    throw std::runtime_error("getmetatable expects a value");
                }
                ValuePtr metatable = ctx.get_metatable(args[0]);
                if (metatable) {
                    return metatable;
                }
                return std::make_shared<Value>();
            }
        );

        register_native(
            "setmetatable",
            [](Context& ctx,
               std::vector<ValuePtr>& args) -> ValuePtr {
                if (args.size() < 2) {
                    throw std::runtime_error("setmetatable expects value and metatable");
                }
                ctx.set_metatable(args[0], args[1]);
                return args[0];
            }
        );
    }

    ValuePtr eval_binary(const NodePtr& node) {
        if (node->text == "and") {
            auto lhs = eval(node->children[0]);
            if (!lhs->truthy()) return lhs;
            return eval(node->children[1]);
        }
        if (node->text == "or") {
            auto lhs = eval(node->children[0]);
            if (lhs->truthy()) return lhs;
            return eval(node->children[1]);
        }

        auto lhs = eval(node->children[0]);
        auto rhs = eval(node->children[1]);

        const std::string& op = node->text;

        if (op == "==") {
            return std::make_shared<Value>(table_key_equals(lhs, rhs));
        }
        if (op == "~=") {
            return std::make_shared<Value>(!table_key_equals(lhs, rhs));
        }

        const char* metamethod_name = nullptr;
        if (op == "+") metamethod_name = "__add";
        else if (op == "-") metamethod_name = "__sub";
        else if (op == "*") metamethod_name = "__mul";
        else if (op == "/") metamethod_name = "__div";

        if (metamethod_name) {
            ValuePtr metamethod_result = invoke_binary_metamethod(lhs, rhs, metamethod_name);
            if (metamethod_result) {
                return metamethod_result;
            }
        }

        if (lhs->type == Value::STRING ||
            rhs->type == Value::STRING) {

            if (op == "+") {
                return std::make_shared<Value>(
                    lhs->to_string() +
                    rhs->to_string()
                );
            }

            throw std::runtime_error(
                "invalid string operation"
            );
        }

        const bool lhs_is_number =
            lhs->type == Value::INT || lhs->type == Value::FLOAT;
        const bool rhs_is_number =
            rhs->type == Value::INT || rhs->type == Value::FLOAT;
        if (!lhs_is_number || !rhs_is_number) {
            throw std::runtime_error("arithmetic expects numeric operands");
        }

        double a =
            lhs->type == Value::FLOAT
                ? lhs->float_value
                : static_cast<double>(lhs->int_value);

        double b =
            rhs->type == Value::FLOAT
                ? rhs->float_value
                : static_cast<double>(rhs->int_value);

        if (op == "+") {
            return std::make_shared<Value>(a + b);
        }

        if (op == "-") {
            return std::make_shared<Value>(a - b);
        }

        if (op == "*") {
            return std::make_shared<Value>(a * b);
        }

        if (op == "/") {
            return std::make_shared<Value>(a / b);
        }

        if (op == "<") {
            return std::make_shared<Value>(a < b);
        }
        if (op == "<=") {
            return std::make_shared<Value>(a <= b);
        }
        if (op == ">") {
            return std::make_shared<Value>(a > b);
        }
        if (op == ">=") {
            return std::make_shared<Value>(a >= b);
        }

        throw std::runtime_error(
            "unknown operator"
        );
    }

    ValuePtr eval_unary(const NodePtr& node) {
        if (node->children.empty()) {
            throw std::runtime_error("invalid unary expression");
        }
        ValuePtr value = eval(node->children[0]);
        if (node->text == "not") {
            return std::make_shared<Value>(!value->truthy());
        }
        if (node->text == "-") {
            if (value->type == Value::INT) {
                return std::make_shared<Value>(-value->int_value);
            }
            if (value->type == Value::FLOAT) {
                return std::make_shared<Value>(-value->float_value);
            }
            throw std::runtime_error("unary '-' expects numeric operand");
        }
        throw std::runtime_error("unknown unary operator");
    }

    ValuePtr eval_call(const NodePtr& node) {
        last_call_returns.clear();
        ValuePtr fn;
        size_t arg_start = 0;

        if (!node->children.empty()) {
            if (node->children[0]->type == Node::NODE_IDENT) {
                fn = lookup_name(node->children[0]->text);
                if (!fn) {
                    throw std::runtime_error(
                        "undefined function: " + node->children[0]->text
                    );
                }
            } else {
                fn = eval(node->children[0]);
            }
            arg_start = 1;
        } else {
            fn = lookup_name(node->text);
            if (!fn) {
                throw std::runtime_error(
                    "undefined function: " + node->text
                );
            }
        }

        std::vector<ValuePtr> args;

        for (size_t i = arg_start; i < node->children.size(); ++i) {
            args.push_back(eval(node->children[i]));
        }

        if (fn->type == Value::FUNCTION) {
            ValuePtr ret = fn->function_value(*this, args);
            if (last_call_returns.empty()) {
                last_call_returns.assign(1, ret);
            }
            return ret;
        }

        ValuePtr call_metamethod = lookup_metamethod(fn, "__call");
        if (call_metamethod && call_metamethod->type == Value::FUNCTION) {
            std::vector<ValuePtr> call_args;
            call_args.push_back(fn);
            call_args.insert(call_args.end(), args.begin(), args.end());
            ValuePtr ret = call_metamethod->function_value(*this, call_args);
            if (last_call_returns.empty()) {
                last_call_returns.assign(1, ret);
            }
            return ret;
        }

        throw std::runtime_error(
            node->text + " is not callable"
        );
    }

    std::vector<ValuePtr> eval_expr_list(const std::vector<NodePtr>& nodes) {
        std::vector<ValuePtr> out;
        for (size_t i = 0; i < nodes.size(); ++i) {
            ValuePtr v = eval(nodes[i]);
            if (i + 1 == nodes.size() && nodes[i]->type == Node::NODE_CALL && !last_call_returns.empty()) {
                out.insert(out.end(), last_call_returns.begin(), last_call_returns.end());
            } else {
                out.push_back(v);
            }
        }
        return out;
    }

    void unload_plugins() {
        for (auto& plugin : plugins) {

            if (plugin.descriptor &&
                plugin.descriptor->on_unload) {

                plugin.descriptor->on_unload(
                    reinterpret_cast<qamrpp_context*>(this)
                );
            }

#ifdef _WIN32
            if (plugin.handle) {
                FreeLibrary(
                    static_cast<HMODULE>(plugin.handle)
                );
            }
#else
            if (plugin.handle) {
                dlclose(plugin.handle);
            }
#endif
        }

        plugins.clear();
    }
};

inline ValuePtr Linker::link(Context& ctx) const {
    ValuePtr last = std::make_shared<Value>();
    for (size_t i = 0; i < units_.size(); ++i) {
        const Unit& u = units_[i];
        try {
            last = ctx.run(u.source);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                "link failed for '" + u.path + "': " + e.what()
            );
        }
    }
    return last;
}

} /* namespace qamrpp */

/* ============================================================
 * C plugin API implementation
 * ============================================================ */

extern "C" {

qamrpp_value* qamrpp_value_nil(qamrpp_context*) {
    return reinterpret_cast<qamrpp_value*>(new qamrpp::Value());
}

qamrpp_value* qamrpp_value_bool(qamrpp_context*, int value) {
    return reinterpret_cast<qamrpp_value*>(new qamrpp::Value(value != 0));
}

qamrpp_value* qamrpp_value_int(qamrpp_context*, int64_t value) {
    return reinterpret_cast<qamrpp_value*>(new qamrpp::Value(value));
}

qamrpp_value* qamrpp_value_float(qamrpp_context*, double value) {
    return reinterpret_cast<qamrpp_value*>(new qamrpp::Value(value));
}

qamrpp_value* qamrpp_value_string(qamrpp_context*, const char* value, size_t len) {
    if (!value && len != 0) {
        return nullptr;
    }
    return reinterpret_cast<qamrpp_value*>(new qamrpp::Value(std::string(value ? value : "", len)));
}

qamrpp_value* qamrpp_value_userdata(qamrpp_context*, void* data, void (*destructor)(void*)) {
    auto* v = new qamrpp::Value();
    v->type = qamrpp::Value::USERDATA;
    v->userdata_value = data;
    v->userdata_destructor = destructor;
    return reinterpret_cast<qamrpp_value*>(v);
}

qamrpp_value* qamrpp_value_table(qamrpp_context*) {
    auto* value = new qamrpp::Value();
    value->type = qamrpp::Value::TABLE;
    return reinterpret_cast<qamrpp_value*>(value);
}

qamrpp_value_type qamrpp_value_get_type(const qamrpp_value* value) {
    if (!value) {
        return QAMRPP_TYPE_NIL;
    }
    return static_cast<qamrpp_value_type>(reinterpret_cast<const qamrpp::Value*>(value)->type);
}

int qamrpp_value_as_bool(const qamrpp_value* value) {
    if (!value) return 0;
    return reinterpret_cast<const qamrpp::Value*>(value)->truthy() ? 1 : 0;
}

int64_t qamrpp_value_as_int(const qamrpp_value* value) {
    if (!value) return 0;
    const auto* v = reinterpret_cast<const qamrpp::Value*>(value);
    if (v->type == qamrpp::Value::INT) return v->int_value;
    if (v->type == qamrpp::Value::FLOAT) return static_cast<int64_t>(v->float_value);
    if (v->type == qamrpp::Value::BOOL) return v->bool_value ? 1 : 0;
    return 0;
}

double qamrpp_value_as_float(const qamrpp_value* value) {
    if (!value) return 0.0;
    const auto* v = reinterpret_cast<const qamrpp::Value*>(value);
    if (v->type == qamrpp::Value::FLOAT) return v->float_value;
    if (v->type == qamrpp::Value::INT) return static_cast<double>(v->int_value);
    if (v->type == qamrpp::Value::BOOL) return v->bool_value ? 1.0 : 0.0;
    return 0.0;
}

const char* qamrpp_value_as_string(const qamrpp_value* value, size_t* out_len) {
    if (!value) {
        if (out_len) *out_len = 0;
        return nullptr;
    }
    const auto* v = reinterpret_cast<const qamrpp::Value*>(value);
    if (v->type != qamrpp::Value::STRING) {
        if (out_len) *out_len = 0;
        return nullptr;
    }
    if (out_len) *out_len = v->string_value.size();
    return v->string_value.c_str();
}

void* qamrpp_value_as_userdata(const qamrpp_value* value) {
    if (!value) return nullptr;
    const auto* v = reinterpret_cast<const qamrpp::Value*>(value);
    if (v->type != qamrpp::Value::USERDATA) return nullptr;
    return v->userdata_value;
}

qamrpp_value* qamrpp_table_raw_get(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key) {
    if (!ctx || !table || !key) return nullptr;
    auto* c = reinterpret_cast<qamrpp::Context*>(ctx);
    qamrpp::ValuePtr table_ptr(reinterpret_cast<qamrpp::Value*>(table), [](qamrpp::Value*) {});
    qamrpp::ValuePtr key_ptr(reinterpret_cast<qamrpp::Value*>(key), [](qamrpp::Value*) {});
    try {
        qamrpp::ValuePtr result = c->table_raw_lookup(table_ptr, key_ptr);
        if (!result) {
            return qamrpp_value_nil(ctx);
        }
        return reinterpret_cast<qamrpp_value*>(result.get());
    } catch (const std::exception& e) {
        qamrpp_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT, e.what());
        return nullptr;
    }
}

int qamrpp_table_raw_set(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key, qamrpp_value* value) {
    if (!ctx || !table || !key || !value) return -1;
    auto* c = reinterpret_cast<qamrpp::Context*>(ctx);
    qamrpp::ValuePtr table_ptr(reinterpret_cast<qamrpp::Value*>(table), [](qamrpp::Value*) {});
    qamrpp::ValuePtr key_ptr(reinterpret_cast<qamrpp::Value*>(key), [](qamrpp::Value*) {});
    qamrpp::ValuePtr value_ptr(reinterpret_cast<qamrpp::Value*>(value), [](qamrpp::Value*) {});
    try {
        c->table_raw_set(table_ptr, key_ptr, value_ptr);
        return 0;
    } catch (const std::exception& e) {
        qamrpp_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT, e.what());
        return -1;
    }
}

qamrpp_value* qamrpp_table_get(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key) {
    if (!ctx || !table || !key) return nullptr;
    auto* c = reinterpret_cast<qamrpp::Context*>(ctx);
    qamrpp::ValuePtr table_ptr(reinterpret_cast<qamrpp::Value*>(table), [](qamrpp::Value*) {});
    qamrpp::ValuePtr key_ptr(reinterpret_cast<qamrpp::Value*>(key), [](qamrpp::Value*) {});
    try {
        qamrpp::ValuePtr result = c->table_get(table_ptr, key_ptr);
        if (!result || result->type == qamrpp::Value::NIL) {
            return qamrpp_value_nil(ctx);
        }
        return reinterpret_cast<qamrpp_value*>(result.get());
    } catch (const std::exception& e) {
        qamrpp_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT, e.what());
        return nullptr;
    }
}

int qamrpp_table_set(qamrpp_context* ctx, qamrpp_value* table, qamrpp_value* key, qamrpp_value* value) {
    if (!ctx || !table || !key || !value) return -1;
    auto* c = reinterpret_cast<qamrpp::Context*>(ctx);
    qamrpp::ValuePtr table_ptr(reinterpret_cast<qamrpp::Value*>(table), [](qamrpp::Value*) {});
    qamrpp::ValuePtr key_ptr(reinterpret_cast<qamrpp::Value*>(key), [](qamrpp::Value*) {});
    qamrpp::ValuePtr value_ptr(reinterpret_cast<qamrpp::Value*>(value), [](qamrpp::Value*) {});
    try {
        c->table_set(table_ptr, key_ptr, value_ptr);
        return 0;
    } catch (const std::exception& e) {
        qamrpp_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT, e.what());
        return -1;
    }
}

qamrpp_value* qamrpp_value_get_metatable(qamrpp_context* ctx, qamrpp_value* value) {
    if (!ctx || !value) return nullptr;
    auto* c = reinterpret_cast<qamrpp::Context*>(ctx);
    qamrpp::ValuePtr value_ptr(reinterpret_cast<qamrpp::Value*>(value), [](qamrpp::Value*) {});
    try {
        qamrpp::ValuePtr metatable = c->get_metatable(value_ptr);
        if (!metatable || metatable->type == qamrpp::Value::NIL) {
            return qamrpp_value_nil(ctx);
        }
        return reinterpret_cast<qamrpp_value*>(metatable.get());
    } catch (const std::exception& e) {
        qamrpp_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT, e.what());
        return nullptr;
    }
}

int qamrpp_value_set_metatable(qamrpp_context* ctx, qamrpp_value* value, qamrpp_value* metatable) {
    if (!ctx || !value || !metatable) return -1;
    auto* c = reinterpret_cast<qamrpp::Context*>(ctx);
    qamrpp::ValuePtr value_ptr(reinterpret_cast<qamrpp::Value*>(value), [](qamrpp::Value*) {});
    qamrpp::ValuePtr metatable_ptr(reinterpret_cast<qamrpp::Value*>(metatable), [](qamrpp::Value*) {});
    try {
        c->set_metatable(value_ptr, metatable_ptr);
        return 0;
    } catch (const std::exception& e) {
        qamrpp_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT, e.what());
        return -1;
    }
}

qamrpp_value* qamrpp_error(qamrpp_context*, qamrpp_error_code, const char* message) {
    return reinterpret_cast<qamrpp_value*>(new qamrpp::Value(std::string(message ? message : "error")));
}

void qamrpp_set_error(qamrpp_context* ctx, qamrpp_error_code code, const char* message) {
    if (!ctx) return;
    auto* c = reinterpret_cast<qamrpp::Context*>(ctx);
    c->last_error_code = code;
    c->last_error_message = message ? message : "";
}

void* qamrpp_context_get_userdata(qamrpp_context* ctx) {
    if (!ctx) return nullptr;
    return reinterpret_cast<qamrpp::Context*>(ctx)->c_userdata;
}

void qamrpp_context_set_userdata(qamrpp_context* ctx, void* data) {
    if (!ctx) return;
    reinterpret_cast<qamrpp::Context*>(ctx)->c_userdata = data;
}

qamrpp_value* qamrpp_get_global(qamrpp_context* ctx, const char* name) {
    if (!ctx || !name) return nullptr;
    auto* c = reinterpret_cast<qamrpp::Context*>(ctx);
    qamrpp::ValuePtr v = c->lookup_name(name);
    return v ? reinterpret_cast<qamrpp_value*>(v.get()) : nullptr;
}

void qamrpp_set_global(qamrpp_context* ctx, const char* name, qamrpp_value* value) {
    if (!ctx || !name || !value) return;
    auto* c = reinterpret_cast<qamrpp::Context*>(ctx);
    c->assign_name(name, std::shared_ptr<qamrpp::Value>(reinterpret_cast<qamrpp::Value*>(value)));
}

} // extern "C"

#endif /* QAMRPP_HPP */
