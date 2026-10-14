#ifndef REGEXPPC_H
#define REGEXPPC_H
#include "chomsky3/lexer.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace regexppc {
struct rule {
    std::string pattern, action;
    std::set<std::string> conditions;
    bool eof = false;
};
struct spec {
    std::map<std::string, std::string> definitions;
    std::map<std::string, bool> conditions{{"INITIAL", false}};
    std::vector<rule> rules;
    std::string cursor = "cursor", limit, preamble, trailer;
    bool simd = false;
    bool nodefault = false;
};
std::string trim(const std::string& s);
void skip_space(const std::string& s, std::size_t& p);
std::string action(const std::string& s, std::size_t& p);
std::string normalize(const std::string& s, const spec& cfg, bool re2c,
                      std::set<std::string> expanding = {});
spec parse_re2c(const std::string& s);
spec parse_flex(const std::string& s);
std::string emit(const spec& cfg, const std::string& prefix, bool flex);
std::string transform_re2c(const std::string& s, bool simd);
void run_lua(const std::string& source, spec& cfg);
}
#endif
