#ifndef QAMRPP_COMPILE_2_C_HPP
#define QAMRPP_COMPILE_2_C_HPP

#include <cctype>
#include <limits>
#include <string>
#include <vector>

#include "../include/QaMRpp.hpp"

namespace qamrpp {

class Compile2CPlugin : public Plugin {
public:
    const char* name() const { return "Compile2C"; }

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
        auto c_escape = [](const std::string& text) {
            std::string out;
            out.reserve(text.size() + 8);
            for (size_t i = 0; i < text.size(); ++i) {
                const char ch = text[i];
                switch (ch) {
                    case '\\': out += "\\\\"; break;
                    case '\"': out += "\\\""; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default: out += ch; break;
                }
            }
            return out;
        };
        long long folded = 0;
        const bool has_folded_eval = fold_const_i64(ir, folded);

        std::string out;
        out += "/* qamrpp c_translation; c99 */\n";
        out += "#include <stdio.h>\n";
        out += "#include <stdint.h>\n";
        out += "static const char qamrpp_ir_data[] = \"";
        out += c_escape(ir);
        out += "\";\n";
        out += "size_t qamrpp_ir_len(void){ return ";
        out += std::to_string(ir.size());
        out += "u; }\n";
        out += "const char* qamrpp_ir_ptr(void){ return qamrpp_ir_data; }\n";
        out += "int64_t qamrpp_eval_ir_i64(void){ return ";
        out += has_folded_eval ? std::to_string(folded) : std::string("0");
        out += "LL; }\n";
        out += "int qamrpp_translate_ir(void){\n";
        out += "  puts(qamrpp_ir_data);\n";
        out += "  return (int)qamrpp_ir_len();\n";
        out += "}\n";
        return out;
    }

    void install(Context& ctx) {
        ctx.register_native("compile_to_c", [](Context&, std::vector<ValuePtr>& args) -> ValuePtr {
            const std::string src = args.empty() ? std::string() : args[0]->to_string();
            Compile2CPlugin self;
            return std::make_shared<Value>(self.translate(src));
        });
    }
};

class CTranslationPlugin final : public Compile2CPlugin {
public:
    const char* name() const { return "c_translation"; }
};

} // namespace qamrpp

#endif
