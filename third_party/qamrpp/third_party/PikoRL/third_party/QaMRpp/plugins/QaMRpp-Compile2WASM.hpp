#ifndef QAMRPP_COMPILE_2_WASM_HPP
#define QAMRPP_COMPILE_2_WASM_HPP

#include <cctype>
#include <limits>
#include <string>
#include <vector>

#include "../include/QaMRpp.hpp"

namespace qamrpp {

class Compile2WASMPlugin : public Plugin {
public:
    const char* name() const { return "Compile2WASM"; }

private:
    static bool fold_const_i64(const std::string& src, long long& out) {
        struct Parser {
            const std::string& s;
            size_t p;

            explicit Parser(const std::string& text) : s(text), p(0) {}

            void skip_ws() {
                while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
            }

            bool parse_number(long long& value) {
                skip_ws();
                if (p >= s.size() || !std::isdigit(static_cast<unsigned char>(s[p]))) return false;
                long long acc = 0;
                while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p]))) {
                    const int d = s[p] - '0';
                    if (acc > (std::numeric_limits<long long>::max() - d) / 10) return false;
                    acc = acc * 10 + d;
                    ++p;
                }
                value = acc;
                return true;
            }

            bool parse_factor(long long& value) {
                skip_ws();
                if (p < s.size() && s[p] == '(') {
                    ++p;
                    if (!parse_expr(value)) return false;
                    skip_ws();
                    if (p >= s.size() || s[p] != ')') return false;
                    ++p;
                    return true;
                }
                if (p < s.size() && s[p] == '-') {
                    ++p;
                    long long inner = 0;
                    if (!parse_factor(inner)) return false;
                    if (inner == std::numeric_limits<long long>::min()) return false;
                    value = -inner;
                    return true;
                }
                return parse_number(value);
            }

            bool parse_term(long long& value) {
                if (!parse_factor(value)) return false;
                for (;;) {
                    skip_ws();
                    if (p >= s.size() || (s[p] != '*' && s[p] != '/')) return true;
                    const char op = s[p++];
                    long long rhs = 0;
                    if (!parse_factor(rhs)) return false;
                    if (op == '*') {
                        if (value != 0 && (rhs > std::numeric_limits<long long>::max() / value ||
                                           rhs < std::numeric_limits<long long>::min() / value)) {
                            return false;
                        }
                        value *= rhs;
                    } else {
                        if (rhs == 0) return false;
                        value /= rhs;
                    }
                }
            }

            bool parse_expr(long long& value) {
                if (!parse_term(value)) return false;
                for (;;) {
                    skip_ws();
                    if (p >= s.size() || (s[p] != '+' && s[p] != '-')) return true;
                    const char op = s[p++];
                    long long rhs = 0;
                    if (!parse_term(rhs)) return false;
                    if ((op == '+' && ((rhs > 0 && value > std::numeric_limits<long long>::max() - rhs) ||
                                       (rhs < 0 && value < std::numeric_limits<long long>::min() - rhs))) ||
                        (op == '-' && ((rhs < 0 && value > std::numeric_limits<long long>::max() + rhs) ||
                                       (rhs > 0 && value < std::numeric_limits<long long>::min() + rhs)))) {
                        return false;
                    }
                    value = (op == '+') ? (value + rhs) : (value - rhs);
                }
            }
        };

        Parser parser(src);
        if (!parser.parse_expr(out)) return false;
        parser.skip_ws();
        return parser.p == src.size();
    }

public:
    std::string translate(const std::string& ir) const {
        auto wat_escape = [](const std::string& text) {
            std::string out;
            out.reserve(text.size() + 8);
            for (size_t i = 0; i < text.size(); ++i) {
                const unsigned char ch = static_cast<unsigned char>(text[i]);
                switch (ch) {
                    case '\"': out += "\\22"; break;
                    case '\\': out += "\\5c"; break;
                    case '\n': out += "\\0a"; break;
                    case '\r': out += "\\0d"; break;
                    case '\t': out += "\\09"; break;
                    default:
                        if (ch < 0x20u || ch > 0x7eu) {
                            static const char* hex = "0123456789abcdef";
                            out += '\\';
                            out += hex[(ch >> 4) & 0x0f];
                            out += hex[ch & 0x0f];
                        } else {
                            out += static_cast<char>(ch);
                        }
                        break;
                }
            }
            return out;
        };

        long long folded = 0;
        const bool has_folded_eval = fold_const_i64(ir, folded);

        std::string out;
        out += ";; qamrpp wasm_translation; wat\n";
        out += "(module\n";
        out += "  (memory (export \"memory\") 1)\n";
        out += "  (global $ir_ptr (export \"ir_ptr\") i32 (i32.const 0))\n";
        out += "  (global $ir_len (export \"ir_len\") i32 (i32.const ";
        out += std::to_string(ir.size());
        out += "))\n";
        out += "  (global $ir_const_eval_i64 (export \"ir_const_eval_i64\") i64 (i64.const ";
        out += has_folded_eval ? std::to_string(folded) : std::string("0");
        out += "))\n";
        out += "  (data (i32.const 0) \"";
        out += wat_escape(ir);
        out += "\")\n";
        out += "  (func (export \"qamrpp_ir_ptr\") (result i32)\n";
        out += "    global.get $ir_ptr)\n";
        out += "  (func (export \"qamrpp_ir_len\") (result i32)\n";
        out += "    global.get $ir_len)\n";
        out += "  (func (export \"qamrpp_eval_ir_i64\") (result i64)\n";
        out += "    global.get $ir_const_eval_i64)\n";
        out += "  (func (export \"qamrpp_translate_ir\") (result i32)\n";
        out += "    call 1)\n";
        out += ")\n";
        return out;
    }

    void install(Context& ctx) {
        ctx.register_native("compile_to_wasm", [](Context&, std::vector<ValuePtr>& args) -> ValuePtr {
            const std::string src = args.empty() ? std::string() : args[0]->to_string();
            Compile2WASMPlugin self;
            return std::make_shared<Value>(self.translate(src));
        });
    }
};

class WASMTranslationPlugin final : public Compile2WASMPlugin {
public:
    const char* name() const { return "wasm_translation"; }
};

} // namespace qamrpp

#endif
