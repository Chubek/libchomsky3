#ifndef QAMRPP_AOT_BACKEND_HPP
#define QAMRPP_AOT_BACKEND_HPP

#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
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
 * QMRB binary format (QaMRpp Binary)
 *
 *   [4]  magic    "QMRB"
 *   [4]  version  uint32_t LE
 *   [4]  entries  uint32_t count
 *   For each entry:
 *     [4]  name_len   uint32_t LE
 *     [n]  name       UTF-8 bytes
 *     [4]  ir_len     uint32_t LE
 *     [n]  ir_text    Jiterati IR (JBL) text
 * ============================================================ */

namespace qamrpp {
namespace aot {

static const std::uint32_t QMRB_MAGIC = 0x42524D51; /* "QMRB" */
static const std::uint32_t QMRB_VERSION = 1u;

struct AOTEntry {
    std::string name;
    std::string ir_text;
};

struct AOTModule {
    std::uint32_t version = QMRB_VERSION;
    std::vector<AOTEntry> entries;
};

/* Binary serializer / deserializer */
class AOTBinary {
public:
    static bool write(const std::string& path, const AOTModule& mod) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.good()) return false;
        w32(out, QMRB_MAGIC);
        w32(out, mod.version);
        w32(out, static_cast<std::uint32_t>(mod.entries.size()));
        for (const auto& e : mod.entries) {
            wstr(out, e.name);
            wstr(out, e.ir_text);
        }
        return out.good();
    }

    static bool read(const std::string& path, AOTModule& mod) {
        std::ifstream in(path, std::ios::binary);
        if (!in.good()) return false;
        if (r32(in) != QMRB_MAGIC) return false;
        mod.version = r32(in);
        std::uint32_t count = r32(in);
        if (!in.good()) return false;
        mod.entries.clear();
        mod.entries.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            AOTEntry e;
            e.name = rstr(in);
            e.ir_text = rstr(in);
            if (!in.good()) return false;
            mod.entries.push_back(std::move(e));
        }
        return true;
    }

private:
    static void w32(std::ofstream& o, std::uint32_t v) {
        o.put(static_cast<char>(v));
        o.put(static_cast<char>(v >> 8));
        o.put(static_cast<char>(v >> 16));
        o.put(static_cast<char>(v >> 24));
    }
    static void wstr(std::ofstream& o, const std::string& s) {
        w32(o, static_cast<std::uint32_t>(s.size()));
        o.write(s.data(), static_cast<std::streamsize>(s.size()));
    }
    static std::uint32_t r32(std::ifstream& in) {
        std::uint32_t v = 0;
        v |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(in.get()));
        v |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(in.get())) << 8;
        v |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(in.get())) << 16;
        v |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(in.get())) << 24;
        return v;
    }
    static std::string rstr(std::ifstream& in) {
        std::uint32_t len = r32(in);
        std::string s(len, '\0');
        in.read(&s[0], static_cast<std::streamsize>(len));
        return s;
    }
};

/* ============================================================
 * QaMRpp AST → Jiterati IR translator
 * ============================================================ */

class IRTranslator {
    jiterati::Module* module_ = nullptr;
    jiterati::Function* func_ = nullptr;
    jiterati::Block* block_ = nullptr;
    std::string error_;

public:
    const std::string& error() const { return error_; }

    std::unique_ptr<jiterati::Module> translate(const NodePtr& ast, const std::string& name) {
        auto mod = std::make_unique<jiterati::Module>(name);
        module_ = mod.get();
        func_ = module_->create_function<std::int64_t()>("__aot_entry");
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
            case Node::NODE_LITERAL: return translate_literal(node, out);
            case Node::NODE_BINARY:  return translate_binary(node, out);
            case Node::NODE_UNARY:   return translate_unary(node, out);
            case Node::NODE_BLOCK: {
                for (size_t i = 0; i < node->children.size(); ++i)
                    if (!translate_expr(node->children[i], out)) return false;
                return true;
            }
            default:
                error_ = "unsupported node: " + std::to_string(node->type);
                return false;
        }
    }

    bool translate_literal(const NodePtr& node, jiterati::Value& out) {
        if (!node->literal) { error_ = "no literal"; return false; }
        switch (node->literal->type) {
            case Value::INT:   out = func_->const_i64(node->literal->int_value); return true;
            case Value::FLOAT: out = func_->const_i64(static_cast<std::int64_t>(node->literal->float_value)); return true;
            case Value::BOOL:  out = func_->const_i64(node->literal->bool_value ? 1 : 0); return true;
            case Value::NIL:   out = func_->const_i64(0); return true;
            default: error_ = "unsupported literal"; return false;
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
        else { error_ = "unsupported binary: " + op; return false; }
        return true;
    }

    bool translate_unary(const NodePtr& node, jiterati::Value& out) {
        if (node->children.empty()) { error_ = "unary needs child"; return false; }
        jiterati::Value operand;
        if (!translate_expr(node->children[0], operand)) return false;
        const std::string& op = node->text;
        if (op == "-")   out = block_->neg(operand);
        else if (op == "not") out = block_->bitwise_not(operand);
        else { error_ = "unsupported unary: " + op; return false; }
        return true;
    }
};

} // namespace aot

/* ============================================================
 * Plugin class
 * ============================================================ */

class AOTBackendPlugin : public Plugin {
public:
    const char* name() const override { return "AOTBackend"; }

    void install(Context& ctx) override {
        /* aot_compile(source, output_path) */
        ctx.register_native("aot_compile", [](Context& c, std::vector<ValuePtr>& args) -> ValuePtr {
            if (args.size() < 2)
                return std::make_shared<Value>(std::string("aot_compile: expected (source, path)"));
            std::string source = args[0]->to_string();
            std::string path = args[1]->to_string();

            Parser parser(source);
            NodePtr ast = parser.parse();
            if (!ast) return std::make_shared<Value>(std::string("aot_compile: parse failed"));

            aot::IRTranslator translator;
            auto mod = translator.translate(ast, "main");
            if (!mod) return std::make_shared<Value>(std::string("aot_compile: " + translator.error()));

            aot::AOTModule aot_mod;
            aot::AOTEntry entry;
            entry.name = "main";
            entry.ir_text = mod->to_string();
            aot_mod.entries.push_back(entry);

            if (!aot::AOTBinary::write(path, aot_mod))
                return std::make_shared<Value>(std::string("aot_compile: write failed"));
            return std::make_shared<Value>(true);
        });

        /* aot_compile_multi(entries_table, output_path) */
        ctx.register_native("aot_compile_multi", [](Context& c, std::vector<ValuePtr>& args) -> ValuePtr {
            if (args.size() < 2)
                return std::make_shared<Value>(std::string("aot_compile_multi: expected (table, path)"));
            if (args[0]->type != Value::TABLE)
                return std::make_shared<Value>(std::string("aot_compile_multi: arg 1 must be table"));

            std::string path = args[1]->to_string();
            aot::AOTModule aot_mod;

            for (auto& kv : args[0]->table_entries) {
                std::string name = kv.first->to_string();
                std::string source = kv.second->to_string();

                Parser parser(source);
                NodePtr ast = parser.parse();
                if (!ast) continue;

                aot::IRTranslator translator;
                auto mod = translator.translate(ast, name);
                if (!mod) continue;

                aot::AOTEntry entry;
                entry.name = name;
                entry.ir_text = mod->to_string();
                aot_mod.entries.push_back(entry);
            }

            if (!aot::AOTBinary::write(path, aot_mod))
                return std::make_shared<Value>(std::string("aot_compile_multi: write failed"));
            return std::make_shared<Value>(true);
        });

        /* aot_load(bin_path) — load, JIT-compile, and execute each entry */
        ctx.register_native("aot_load", [](Context& c, std::vector<ValuePtr>& args) -> ValuePtr {
            if (args.empty())
                return std::make_shared<Value>(std::string("aot_load: expected (path)"));
            std::string path = args[0]->to_string();

            aot::AOTModule aot_mod;
            if (!aot::AOTBinary::read(path, aot_mod))
                return std::make_shared<Value>(std::string("aot_load: failed to read " + path));

            ValuePtr last_result;
            for (auto& entry : aot_mod.entries) {
                std::string error;
                auto mod = jiterati::parse_jbl(entry.ir_text, &error);
                if (!mod || !error.empty()) continue;

                if (mod->functions().empty()) continue;
                auto* fn = mod->functions().front().get();

                try {
                    jiterati::be::AMD64Backend backend;
                    auto compiled = backend.compile_function(*fn);
                    auto* entry_fn = compiled->as<std::int64_t (*)()>();
                    last_result = std::make_shared<Value>(entry_fn());
                } catch (const std::exception&) {
                    continue;
                }
            }
            return last_result ? last_result : std::make_shared<Value>();
        });

        /* aot_load_file(bin_path) — load, JIT-compile, and register functions */
        ctx.register_native("aot_load_file", [&ctx](Context& c, std::vector<ValuePtr>& args) -> ValuePtr {
            if (args.empty())
                return std::make_shared<Value>(std::string("aot_load_file: expected (path)"));
            std::string path = args[0]->to_string();

            aot::AOTModule aot_mod;
            if (!aot::AOTBinary::read(path, aot_mod))
                return std::make_shared<Value>(std::string("aot_load_file: failed to read " + path));

            for (auto& entry : aot_mod.entries) {
                std::string error;
                auto mod = jiterati::parse_jbl(entry.ir_text, &error);
                if (!mod || !error.empty()) continue;
                if (mod->functions().empty()) continue;
                auto* fn = mod->functions().front().get();

                try {
                    jiterati::be::AMD64Backend backend;
                    auto compiled = backend.compile_function(*fn);
                    auto* entry_fn_ptr = compiled->as<std::int64_t (*)()>();

                    /* Capture the compiled function in a shared_ptr so it lives */
                    auto compiled_shared = std::make_shared<jiterati::CompiledFunction>(std::move(*compiled));
                    auto* captured_fn = compiled_shared->as<std::int64_t (*)()>();

                    ctx.register_native(entry.name,
                        [captured_fn, compiled_shared](Context&, std::vector<ValuePtr>&) -> ValuePtr {
                            return std::make_shared<Value>(captured_fn());
                        });
                } catch (const std::exception&) {
                    continue;
                }
            }
            return std::make_shared<Value>(true);
        });

        /* aot_inspect(bin_path) */
        ctx.register_native("aot_inspect", [](Context& c, std::vector<ValuePtr>& args) -> ValuePtr {
            (void)c;
            if (args.empty())
                return std::make_shared<Value>(std::string("aot_inspect: expected (path)"));
            std::string path = args[0]->to_string();

            aot::AOTModule aot_mod;
            if (!aot::AOTBinary::read(path, aot_mod))
                return std::make_shared<Value>(std::string("aot_inspect: failed to read " + path));

            auto t = Value::make_table();
            t->table_entries.push_back({
                std::make_shared<Value>(std::string("version")),
                std::make_shared<Value>(static_cast<std::int64_t>(aot_mod.version))
            });
            t->table_entries.push_back({
                std::make_shared<Value>(std::string("entry_count")),
                std::make_shared<Value>(static_cast<std::int64_t>(aot_mod.entries.size()))
            });

            auto entries = Value::make_table();
            for (size_t i = 0; i < aot_mod.entries.size(); ++i) {
                auto info = Value::make_table();
                info->table_entries.push_back({
                    std::make_shared<Value>(std::string("name")),
                    std::make_shared<Value>(aot_mod.entries[i].name)
                });
                info->table_entries.push_back({
                    std::make_shared<Value>(std::string("ir_size")),
                    std::make_shared<Value>(static_cast<std::int64_t>(aot_mod.entries[i].ir_text.size()))
                });
                entries->table_entries.push_back({
                    std::make_shared<Value>(static_cast<std::int64_t>(i + 1)),
                    info
                });
            }
            t->table_entries.push_back({
                std::make_shared<Value>(std::string("entries")),
                entries
            });
            return t;
        });
    }
};

} // namespace qamrpp

#endif /* QAMRPP_AOT_BACKEND_HPP */
