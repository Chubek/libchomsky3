#ifndef QAMRPP_COMPILE_2_BYTECODE_HPP
#define QAMRPP_COMPILE_2_BYTECODE_HPP

#include <cctype>
#include <limits>
#include <string>
#include <vector>

#include "../include/QaMRpp.hpp"

namespace qamrpp {

class Compile2BytecodePlugin : public Plugin {
public:
    const char* name() const { return "Compile2Bytecode"; }

    std::string translate(const std::string& ir) const {
        struct ConstExprParser {
            const std::string& src;
            size_t pos;

            explicit ConstExprParser(const std::string& s) : src(s), pos(0) {}

            void skip_ws() {
                while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) ++pos;
            }

            bool parse_number(long long& out) {
                skip_ws();
                if (pos >= src.size() || !std::isdigit(static_cast<unsigned char>(src[pos]))) return false;
                long long value = 0;
                while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) {
                    const int d = src[pos] - '0';
                    if (value > (std::numeric_limits<long long>::max() - d) / 10) return false;
                    value = value * 10 + d;
                    ++pos;
                }
                out = value;
                return true;
            }

            bool parse_factor(long long& out) {
                skip_ws();
                if (pos < src.size() && src[pos] == '(') {
                    ++pos;
                    if (!parse_expr(out)) return false;
                    skip_ws();
                    if (pos >= src.size() || src[pos] != ')') return false;
                    ++pos;
                    return true;
                }
                if (pos < src.size() && src[pos] == '-') {
                    ++pos;
                    long long value = 0;
                    if (!parse_factor(value)) return false;
                    if (value == std::numeric_limits<long long>::min()) return false;
                    out = -value;
                    return true;
                }
                return parse_number(out);
            }

            bool parse_term(long long& out) {
                if (!parse_factor(out)) return false;
                for (;;) {
                    skip_ws();
                    if (pos >= src.size() || (src[pos] != '*' && src[pos] != '/')) return true;
                    const char op = src[pos++];
                    long long rhs = 0;
                    if (!parse_factor(rhs)) return false;
                    if (op == '*') {
                        if (out != 0 && (rhs > std::numeric_limits<long long>::max() / out ||
                                         rhs < std::numeric_limits<long long>::min() / out)) {
                            return false;
                        }
                        out *= rhs;
                    } else {
                        if (rhs == 0) return false;
                        out /= rhs;
                    }
                }
            }

            bool parse_expr(long long& out) {
                if (!parse_term(out)) return false;
                for (;;) {
                    skip_ws();
                    if (pos >= src.size() || (src[pos] != '+' && src[pos] != '-')) return true;
                    const char op = src[pos++];
                    long long rhs = 0;
                    if (!parse_term(rhs)) return false;
                    if ((op == '+' && ((rhs > 0 && out > std::numeric_limits<long long>::max() - rhs) ||
                                       (rhs < 0 && out < std::numeric_limits<long long>::min() - rhs))) ||
                        (op == '-' && ((rhs < 0 && out > std::numeric_limits<long long>::max() + rhs) ||
                                       (rhs > 0 && out < std::numeric_limits<long long>::min() + rhs)))) {
                        return false;
                    }
                    out = (op == '+') ? (out + rhs) : (out - rhs);
                }
            }

            bool done() {
                skip_ws();
                return pos == src.size();
            }
        };

        long long folded = 0;
        ConstExprParser parser(ir);
        const bool has_folded_eval = parser.parse_expr(folded) && parser.done();

        std::string out;
        out += "QBC1\n";
        out += "; qamrpp bytecode_translation\n";
        out += "META_IR_LEN ";
        out += std::to_string(ir.size());
        out += "\n";
        out += "DATA_IR\n";
        for (size_t i = 0; i < ir.size(); ++i) {
            out += "PUSHI8 ";
            out += std::to_string(static_cast<unsigned int>(static_cast<unsigned char>(ir[i])));
            out += "\n";
        }
        out += "END_DATA\n";
        out += "FUNC qamrpp_translate_ir_len\n";
        out += "PUSHI32 ";
        out += std::to_string(ir.size());
        out += "\nRET\n";
        out += "FUNC qamrpp_eval_ir\n";
        out += "PUSHI64 ";
        out += has_folded_eval ? std::to_string(folded) : std::string("0");
        out += "\nRET\n";
        return out;
    }

    void install(Context& ctx) {
        ctx.register_native("compile_to_bytecode", [](Context&, std::vector<ValuePtr>& args) -> ValuePtr {
            const std::string src = args.empty() ? std::string() : args[0]->to_string();
            Compile2BytecodePlugin self;
            return std::make_shared<Value>(self.translate(src));
        });
    }
};

class BytecodeTranslationPlugin final : public Compile2BytecodePlugin {
public:
    const char* name() const { return "bytecode_translation"; }
};

} // namespace qamrpp

#endif
