#include "regexppc.h"
#include <cctype>
#include <stdexcept>

namespace regexppc {
std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
void skip_space(const std::string& s, std::size_t& p) {
    for (;;) {
        while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
        if (s.compare(p, 2, "//") == 0) {
            auto end = s.find('\n', p); p = end == std::string::npos ? s.size() : end;
        } else if (s.compare(p, 2, "/*") == 0) {
            auto end = s.find("*/", p + 2);
            if (end == std::string::npos) throw std::runtime_error("unterminated comment");
            p = end + 2;
        } else return;
    }
}
std::string action(const std::string& s, std::size_t& p) {
    skip_space(s, p);
    if (p >= s.size() || s[p] != '{') throw std::runtime_error("expected braced action");
    std::size_t start = ++p;
    int depth = 1;
    while (p < s.size()) {
        if (s[p] == '"' || s[p] == '\'') {
            char quote = s[p++]; bool closed = false;
            while (p < s.size()) {
                char c = s[p++];
                if (c == '\\' && p < s.size()) ++p;
                else if (c == quote) { closed = true; break; }
            }
            if (!closed) throw std::runtime_error("unterminated action string");
        } else if (s.compare(p, 2, "//") == 0 || s.compare(p, 2, "/*") == 0) {
            skip_space(s, p);
        } else {
            char c = s[p++];
            if (c == '{') ++depth;
            if (c == '}' && --depth == 0) return s.substr(start, p - start - 1);
        }
    }
    throw std::runtime_error("unterminated action");
}
std::string normalize(const std::string& s, const spec& cfg, bool re2c, std::set<std::string> expanding) {
    std::string out;
    for (std::size_t p = 0; p < s.size();) {
        char c = s[p];
        if (c == '[') {
            auto start = p++;
            if (p < s.size() && s[p] == '^') ++p;
            if (p < s.size() && s[p] == ']') ++p;
            while (p < s.size() && s[p] != ']') {
                if (s[p++] == '\\' && p < s.size()) ++p;
            }
            if (p == s.size()) throw std::runtime_error("unterminated character class");
            out += s.substr(start, ++p - start);
        } else if (c == '"' || (re2c && c == '\'')) {
            char quote = s[p++]; bool closed = false;
            out += '(';
            while (p < s.size()) {
                char ch = s[p++];
                if (ch == quote) { closed = true; break; }
                if (ch == '\\') {
                    if (p == s.size()) throw std::runtime_error("incomplete escape");
                    out += ch; out += s[p++];
                } else {
                    if (std::string(".[](){}*+?|^$\\").find(ch) != std::string::npos) out += '\\';
                    out += ch;
                }
            }
            if (!closed) throw std::runtime_error("unterminated regex string");
            out += ')';
        } else if (c == '\\') {
            out += s[p++];
            if (p == s.size()) throw std::runtime_error("incomplete escape");
            out += s[p++];
        } else if ((re2c && (std::isalpha(static_cast<unsigned char>(c)) || c == '_')) ||
                   (!re2c && c == '{' && p + 1 < s.size() &&
                    (std::isalpha(static_cast<unsigned char>(s[p + 1])) || s[p + 1] == '_'))) {
            if (!re2c) ++p;
            auto start = p++;
            while (p < s.size() && (std::isalnum(static_cast<unsigned char>(s[p])) || s[p] == '_')) ++p;
            auto name = s.substr(start, p - start);
            if (!re2c && (p == s.size() || s[p++] != '}')) throw std::runtime_error("invalid definition reference");
            auto def = cfg.definitions.find(name);
            if (def == cfg.definitions.end()) throw std::runtime_error("undefined regex: " + name);
            if (!expanding.insert(name).second) throw std::runtime_error("cyclic regex definition: " + name);
            out += "(" + normalize(def->second, cfg, re2c, expanding) + ")";
            expanding.erase(name);
        } else {
            ++p;
            if (!re2c || !std::isspace(static_cast<unsigned char>(c))) out += c;
        }
    }
    if (trim(s) == "*" && re2c) return "[\\x00-\\xff]";
    return out;
}
}
