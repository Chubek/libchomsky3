#ifndef QAMRPP_JIT_BACKEND_HPP
#define QAMRPP_JIT_BACKEND_HPP

#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <QaMRpp.hpp>
#include <Jiterati.hpp>
#include <Jiterati-BE.hpp>
#include "BE/AMD64/Backend.hpp"

/* ============================================================
 * QaMRpp AST → Jiterati IR translator
 * ============================================================ */

namespace qamrpp {
namespace jit {

class IRTranslator {
    jiterati::Module* module_ = nullptr;
    jiterati::Function* func_ = nullptr;
    jiterati::Block* block_ = nullptr;
    std::string error_;

public:
    const std::string& error() const { return error_; }

    /* Translate a QaMRpp AST expression into a Jiterati Module.
     * Returns nullptr on failure (check error()). */
    std::unique_ptr<jiterati::Module> translate(const NodePtr& ast) {
        auto mod = std::make_unique<jiterati::Module>("qamrpp_jit");
        module_ = mod.get();

        func_ = module_->create_function<std::int64_t()>("__jit_entry");
        block_ = func_->create_block("entry");

        jiterati::Value jv;
        if (!translate_expr(ast, jv)) return nullptr;

        block_->ret(jv);
        return mod;
    }

private:
    bool translate_expr(const NodePtr& node, jiterati::Value& out) {
        if (!node) { error_ = "null node"; return false; }

        switch (node->type) {
            case Node::NODE_LITERAL:
                return translate_literal(node, out);

            case Node::NODE_BINARY:
                return translate_binary(node, out);

            case Node::NODE_UNARY:
                return translate_unary(node, out);

            case Node::NODE_BLOCK:
                return translate_block(node, out);

            case Node::NODE_CALL:
                return translate_call(node, out);

            default:
                error_ = "unsupported node type: " + std::to_string(node->type);
                return false;
        }
    }

    bool translate_literal(const NodePtr& node, jiterati::Value& out) {
        if (!node->literal) { error_ = "literal has no value"; return false; }
        switch (node->literal->type) {
            case Value::INT:
                out = func_->const_i64(node->literal->int_value);
                return true;
            case Value::FLOAT:
                out = func_->const_i64(static_cast<std::int64_t>(node->literal->float_value));
                return true;
            case Value::BOOL:
                out = func_->const_i64(node->literal->bool_value ? 1 : 0);
                return true;
            case Value::NIL:
                out = func_->const_i64(0);
                return true;
            default:
                error_ = "unsupported literal type";
                return false;
        }
    }

    bool translate_binary(const NodePtr& node, jiterati::Value& out) {
        if (node->children.size() < 2) { error_ = "binary needs 2 children"; return false; }
        jiterati::Value lhs, rhs;
        if (!translate_expr(node->children[0], lhs)) return false;
        if (!translate_expr(node->children[1], rhs)) return false;

        const std::string& op = node->text;
        if (op == "+")       out = block_->add(lhs, rhs);
        else if (op == "-")  out = block_->sub(lhs, rhs);
        else if (op == "*")  out = block_->mul(lhs, rhs);
        else if (op == "/")  out = block_->sdiv(lhs, rhs);
        else if (op == "%")  out = block_->srem(lhs, rhs);
        else if (op == "&")  out = block_->bitwise_and(lhs, rhs);
        else if (op == "|")  out = block_->bitwise_or(lhs, rhs);
        else if (op == "^")  out = block_->bitwise_xor(lhs, rhs);
        else if (op == "<<") out = block_->shl(lhs, rhs);
        else if (op == ">>") out = block_->ashr(lhs, rhs);
        else if (op == "==") out = block_->icmp_eq(lhs, rhs);
        else if (op == "~=") out = block_->icmp_ne(lhs, rhs);
        else if (op == "<")  out = block_->icmp_slt(lhs, rhs);
        else if (op == "<=") out = block_->icmp_sle(lhs, rhs);
        else if (op == ">")  out = block_->icmp_sgt(lhs, rhs);
        else if (op == ">=") out = block_->icmp_sge(lhs, rhs);
        else {
            error_ = "unsupported binary op: " + op;
            return false;
        }
        return true;
    }

    bool translate_unary(const NodePtr& node, jiterati::Value& out) {
        if (node->children.empty()) { error_ = "unary needs child"; return false; }
        jiterati::Value operand;
        if (!translate_expr(node->children[0], operand)) return false;

        const std::string& op = node->text;
        if (op == "-")   out = block_->neg(operand);
        else if (op == "not") out = block_->bitwise_not(operand);
        else {
            error_ = "unsupported unary op: " + op;
            return false;
        }
        return true;
    }

    bool translate_block(const NodePtr& node, jiterati::Value& out) {
        for (size_t i = 0; i < node->children.size(); ++i) {
            if (!translate_expr(node->children[i], out)) return false;
        }
        return true;
    }

    bool translate_call(const NodePtr& node, jiterati::Value& out) {
        (void)node;
        out = func_->const_i64(0);
        return true;
    }
};

} // namespace jit

/* ============================================================
 * Plugin class
 * ============================================================ */

class JITBackendPlugin : public Plugin {
public:
    const char* name() const override { return "JITBackend"; }

    void install(Context& ctx) override {
        /* jit(expression) — JIT-compile and execute, returning the result */
        ctx.register_native("jit", [](Context& c, std::vector<ValuePtr>& args) -> ValuePtr {
            if (args.empty()) {
                return std::make_shared<Value>(std::string("jit: expected 1 argument"));
            }
            std::string source = args[0]->to_string();

            Parser parser(source);
            NodePtr ast = parser.parse();
            if (!ast) {
                return std::make_shared<Value>(std::string("jit: parse failed"));
            }

            jit::IRTranslator translator;
            auto mod = translator.translate(ast);
            if (!mod) {
                return std::make_shared<Value>(std::string("jit: " + translator.error()));
            }

            try {
                jiterati::be::AMD64Backend backend;
                auto* fn = mod->functions().front().get();
                auto compiled = backend.compile_function(*fn);
                auto* entry_fn = compiled->as<std::int64_t (*)()>();
                std::int64_t result = entry_fn();
                return std::make_shared<Value>(result);
            } catch (const std::exception& e) {
                return std::make_shared<Value>(std::string("jit: ") + e.what());
            }
        });

        /* jit_compile(source) — JIT-compile and return IR text for inspection */
        ctx.register_native("jit_compile", [](Context& c, std::vector<ValuePtr>& args) -> ValuePtr {
            if (args.empty()) {
                return std::make_shared<Value>(std::string("jit_compile: expected 1 argument"));
            }
            std::string source = args[0]->to_string();

            Parser parser(source);
            NodePtr ast = parser.parse();
            if (!ast) {
                return std::make_shared<Value>(std::string("jit_compile: parse failed"));
            }

            jit::IRTranslator translator;
            auto mod = translator.translate(ast);
            if (!mod) {
                return std::make_shared<Value>(std::string("jit_compile: " + translator.error()));
            }

            auto t = Value::make_table();
            t->table_entries.push_back({
                std::make_shared<Value>(std::string("ok")),
                std::make_shared<Value>(true)
            });
            t->table_entries.push_back({
                std::make_shared<Value>(std::string("ir")),
                std::make_shared<Value>(mod->to_string())
            });
            return t;
        });
    }
};

} // namespace qamrpp

#endif /* QAMRPP_JIT_BACKEND_HPP */
