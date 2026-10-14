#include "regexppc.h"
#include <cctype>
#include <stdexcept>

namespace regexppc {
spec parse_re2c(const std::string& s) {
    spec cfg;
    std::size_t p = 0;
    while (true) {
        skip_space(s, p);
        if (p == s.size()) break;
        if (s.compare(p, 12, "regexppc:lua") == 0) {
            p += 12; run_lua(action(s, p), cfg); continue;
        }
        if (s.compare(p, 9, "regexppc:") == 0 || s.compare(p, 5, "re2c:") == 0) {
            auto end = s.find(';', p), eq = s.find('=', p);
            if (end == std::string::npos || eq == std::string::npos || eq > end)
                throw std::runtime_error("invalid scanner directive");
            auto key = trim(s.substr(p, eq - p));
            key = key.substr(key.find(':') + 1);
            auto value = trim(s.substr(eq + 1, end - eq - 1));
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
            if (key == "define:YYCURSOR") cfg.cursor = value;
            else if (key == "define:YYLIMIT") cfg.limit = value;
            else if (key == "yyfill:enable" && value == "0") {}
            else if (key == "simd") cfg.simd = value == "1";
            else throw std::runtime_error("unsupported directive: " + key);
            p = end + 1; continue;
        }
        auto start = p;
        char quote = 0; bool cls = false;
        while (p < s.size()) {
            char c = s[p];
            if (c == '\\') { p += 2; continue; }
            if (quote) { if (c == quote) quote = 0; ++p; continue; }
            if (c == '"' || c == '\'') { quote = c; ++p; continue; }
            if (c == '[') cls = true;
            if (c == ']') cls = false;
            if (!cls && (c == '=' || c == ';')) break;
            if (!cls && c == '{') {
                if (p + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[p + 1]))) {
                    auto end = s.find('}', p);
                    if (end == std::string::npos) throw std::runtime_error("unterminated repetition");
                    p = end + 1; continue;
                }
                break;
            }
            ++p;
        }
        if (p >= s.size()) {
            if (quote || cls) throw std::runtime_error("unterminated regex string or character class");
            throw std::runtime_error("expected definition or rule action");
        }
        auto expression = trim(s.substr(start, p - start));
        if (s[p] == '=') {
            auto end = ++p;
            char def_quote = 0; bool def_class = false;
            for (; end < s.size(); ++end) {
                char c = s[end];
                if (c == '\\') { ++end; continue; }
                if (def_quote) { if (c == def_quote) def_quote = 0; continue; }
                if (c == '"' || c == '\'') { def_quote = c; continue; }
                if (c == '[') def_class = true;
                if (c == ']') def_class = false;
                if (!def_class && c == ';') break;
            }
            if (end >= s.size()) throw std::runtime_error("unterminated definition");
            if (expression.empty() || expression.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos)
                throw std::runtime_error("invalid definition name");
            if (!cfg.definitions.emplace(expression, trim(s.substr(p, end - p))).second)
                throw std::runtime_error("duplicate definition: " + expression);
            p = end + 1;
        } else if (s[p] == '{') {
            rule r; r.pattern = expression; r.action = action(s, p); cfg.rules.push_back(r);
        } else throw std::runtime_error("rule requires an action");
    }
    for (auto& r : cfg.rules) r.pattern = normalize(r.pattern, cfg, true);
    return cfg;
}
std::string transform_re2c(const std::string& s, bool simd) {
    std::string out;
    std::size_t p = 0, block = 0;
    while (true) {
        auto begin = s.find("/*!regexppc", p);
        if (begin == std::string::npos) { out += s.substr(p); break; }
        out += s.substr(p, begin - p);
        auto end = s.find("*/", begin + 11);
        if (end == std::string::npos) throw std::runtime_error("unterminated RegexPPC block");
        auto cfg = parse_re2c(s.substr(begin + 11, end - begin - 11));
        cfg.simd = cfg.simd || simd;
        out += emit(cfg, "regexppc_" + std::to_string(block++), false);
        p = end + 2;
    }
    if (!block) throw std::runtime_error("no /*!regexppc block found");
    if (simd || out.find("regexppc::skip_run") != std::string::npos)
        out = "#include <regexppc/runtime.hpp>\n" + out;
    return out;
}
}
