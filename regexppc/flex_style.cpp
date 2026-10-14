#include "regexppc.h"
#include <sstream>
#include <stdexcept>

namespace regexppc {
spec parse_flex(const std::string& s) {
    spec cfg;
    auto first = s.find("%%");
    if (first == std::string::npos) throw std::runtime_error("Flex input needs %% before rules");
    auto second = s.find("%%", first + 2);
    std::string defs = s.substr(0, first);
    auto a = defs.find("%{");
    if (a != std::string::npos) {
        auto b = defs.find("%}", a + 2);
        if (b == std::string::npos) throw std::runtime_error("unterminated %{ preamble");
        cfg.preamble = defs.substr(a + 2, b - a - 2); defs.erase(a, b + 2 - a);
    }
    std::istringstream lines(defs);
    std::string line;
    while (std::getline(lines, line)) {
        line = trim(line); if (line.empty() || line.compare(0, 2, "//") == 0) continue;
        if (line.compare(0, 4, "%lua") == 0) {
            std::string chunk = line.substr(4), more;
            while (std::getline(lines, more)) chunk += '\n' + more;
            std::size_t pos = 0;
            run_lua(action(chunk, pos), cfg);
            defs = chunk.substr(pos);
            lines.clear(); lines.str(defs);
            continue;
        }
        std::istringstream words(line); std::string name; words >> name;
        if (name == "%s" || name == "%x") {
            std::string condition;
            while (words >> condition) {
                if (!cfg.conditions.emplace(condition, name == "%x").second)
                    throw std::runtime_error("duplicate start condition: " + condition);
            }
        } else if (name == "%option") {
            std::string option;
            while (words >> option) {
                if (option == "nodefault") cfg.nodefault = true;
                else if (option != "noyywrap" && option != "8bit" && option != "stack" && option != "noinput" && option != "nounput")
                    throw std::runtime_error("unsupported Flex option: " + option);
            }
        } else if (!name.empty() && name[0] == '%') throw std::runtime_error("unsupported Flex declaration: " + name);
        else {
            std::string expression; std::getline(words, expression);
            if (!cfg.definitions.emplace(name, trim(expression)).second) throw std::runtime_error("duplicate definition: " + name);
        }
    }
    std::string rules = s.substr(first + 2, second == std::string::npos ? std::string::npos : second - first - 2);
    std::size_t p = 0;
    while (true) {
        skip_space(rules, p); if (p == rules.size()) break;
        rule r;
        if (rules[p] == '<' && rules.compare(p, 7, "<<EOF>>") != 0) {
            auto end = rules.find('>', ++p);
            if (end == std::string::npos) throw std::runtime_error("unterminated start condition list");
            std::istringstream names(rules.substr(p, end - p)); std::string name;
            while (std::getline(names, name, ',')) {
                name = trim(name);
                if (name == "*") for (const auto& c : cfg.conditions) r.conditions.insert(c.first);
                else {
                    if (!cfg.conditions.count(name)) throw std::runtime_error("unknown start condition: " + name);
                    r.conditions.insert(name);
                }
            }
            p = end + 1;
        }
        auto start = p; bool cls = false; char quote = 0;
        while (p < rules.size()) {
            char c = rules[p];
            if (c == '\\') { p += 2; continue; }
            if (quote) { if (c == quote) quote = 0; ++p; continue; }
            if (c == '"') quote = c;
            if (c == '[') cls = true;
            if (c == ']') cls = false;
            if (!cls && !quote && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) break;
            ++p;
        }
        r.pattern = rules.substr(start, p - start);
        r.eof = r.pattern == "<<EOF>>";
        skip_space(rules, p);
        if (p < rules.size() && rules[p] == '{') r.action = action(rules, p);
        else {
            auto end = rules.find('\n', p); if (end == std::string::npos) end = rules.size();
            r.action = trim(rules.substr(p, end - p)); p = end;
            if (r.action == "|") throw std::runtime_error("shared Flex actions require explicit braces");
        }
        if (r.pattern.empty()) throw std::runtime_error("empty Flex rule");
        if (!r.eof) r.pattern = normalize(r.pattern, cfg, false);
        cfg.rules.push_back(r);
    }
    if (second != std::string::npos) cfg.trailer = s.substr(second + 2);
    return cfg;
}
}
