#include "regexppc.h"
#include "chomsky3/lexer_simd.h"
#include <sstream>
#include <stdexcept>

namespace regexppc {
namespace {
void tables(std::ostream& out, const chomsky3::lexer_dfa& dfa, const std::string& name) {
    out << "static const int " << name << "_next[][256] = {\n";
    for (const auto& row : dfa.transitions) {
        out << "{"; for (int target : row) out << target << ','; out << "},\n";
    }
    out << "};\nstatic const int " << name << "_accept[] = {";
    for (int accept : dfa.accepts) out << accept << ',';
    out << "};\n";
}
void scanner(std::ostream& out, const spec& cfg, const chomsky3::lexer_dfa& dfa,
             const std::string& name, const std::vector<int>& indices, bool flex) {
    tables(out, dfa, name);
    out << "{\nconst unsigned char *rp = (const unsigned char *)(" << cfg.cursor << ");\n"
        << "const unsigned char *rp_begin = rp, *rp_best = rp;\nint rp_state = 0, rp_rule = -1;\n";
    if (!cfg.limit.empty()) out << "const unsigned char *rp_end = (const unsigned char *)(" << cfg.limit << ");\n";
    out << "for (;;) {\n";
    if (!cfg.limit.empty()) out << "if (rp == rp_end) break;\n";
    if (cfg.simd && !cfg.limit.empty()) {
        out << "switch (rp_state) {\n";
        for (std::size_t s = 0; s < dfa.transitions.size(); ++s) {
            unsigned char lo, hi;
            if (dfa.accepts[s] >= 0 && chomsky3_lexer_simd_range(dfa.transitions[s].data(), static_cast<int>(s), &lo, &hi))
                out << "case " << s << ": rp = regexppc::skip_run(rp, rp_end, " << static_cast<unsigned>(lo) << ", " << static_cast<unsigned>(hi)
                    << "); rp_best = rp; rp_rule = " << dfa.accepts[s] << "; break;\n";
        }
        out << "default: break;\n}\nif (rp == rp_end) break;\n";
    }
    out << "unsigned int rp_byte = *rp;\nint rp_next = " << name << "_next[rp_state][rp_byte];\n"
        << "if (rp_next < 0) break;\n++rp; rp_state = rp_next;\n"
        << "if (" << name << "_accept[rp_state] >= 0) { rp_rule = " << name << "_accept[rp_state]; rp_best = rp; }\n";
    if (cfg.limit.empty()) out << "if (rp_byte == 0) break;\n";
    out << "}\n";
    if (flex) {
        out << "if (rp_rule < 0) { ";
        if (cfg.nodefault) out << "fprintf(stderr, \"RegexPPC scanner jammed\\n\"); exit(1);";
        else out << "putchar((unsigned char)*yycursor++); continue;";
        out << " }\n"
            << "yyleng = (int)(rp_best - rp_begin);\n"
            << "if (rp_best - rp_begin > 2147483647) { fprintf(stderr, \"RegexPPC token too long\\n\"); exit(1); }\n"
            << "if ((unsigned int)yyleng + 1 > yytextcap) { char *p = (char *)realloc(yytext, (unsigned int)yyleng + 1); if (!p) abort(); yytext = p; yytextcap = (unsigned int)yyleng + 1; }\n"
            << "memcpy(yytext, rp_begin, (unsigned int)yyleng); yytext[yyleng] = 0;\n";
    } else out << "(void)rp_begin;\n";
    out << cfg.cursor << " += rp_best - (const unsigned char *)(" << cfg.cursor << ");\n"
        << "switch (rp_rule) {\n";
    for (std::size_t i = 0; i < indices.size(); ++i)
        out << "case " << i << ": {\n" << cfg.rules[indices[i]].action << "\n} break;\n";
    out << "default: break;\n}\n}\n";
}
}
std::string emit(const spec& input, const std::string& prefix, bool flex) {
    spec cfg = input;
    if (cfg.rules.empty()) throw std::runtime_error("scanner has no rules");
    if (cfg.simd && !flex && cfg.limit.empty()) throw std::runtime_error("SIMD requires define:YYLIMIT");
    std::ostringstream out;
    if (!flex) {
        std::vector<std::string> patterns; std::vector<int> indices;
        for (std::size_t i = 0; i < cfg.rules.size(); ++i) { patterns.push_back(cfg.rules[i].pattern); indices.push_back(static_cast<int>(i)); }
        out << "{ /* RegexPPC: longest match, first rule on ties */\n";
        scanner(out, cfg, chomsky3::compile_lexer(patterns), prefix, indices, false);
        out << "}\n";
        return out.str();
    }
    cfg.cursor = "yycursor"; cfg.limit = "yylimit";
    if (cfg.simd) out << "#include <regexppc/runtime.hpp>\n";
    out << "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n" << cfg.preamble << "\n";
    out << "enum {";
    int id = 0;
    for (const auto& c : cfg.conditions) { out << c.first << "=" << id++ << ','; }
    out << "};\nFILE *yyin;\nchar *yytext;\nstatic size_t yytextcap;\nint yyleng;\nstatic int yy_start = INITIAL;\n"
        << "static int yy_stack[256], yy_depth;\n"
        << "static char *yybuffer, *yycursor, *yylimit;\n"
        << "#define BEGIN(s) (yy_start = (s))\n#define YY_START yy_start\n"
        << "#define ECHO fwrite(yytext, 1, (unsigned int)yyleng, stdout)\n"
        << "void yy_push_state(int s) { if (yy_depth == 256) abort(); yy_stack[yy_depth++] = yy_start; BEGIN(s); }\n"
        << "void yy_pop_state(void) { if (!yy_depth) abort(); BEGIN(yy_stack[--yy_depth]); }\n"
        << "int yy_top_state(void) { if (!yy_depth) abort(); return yy_stack[yy_depth-1]; }\n"
        << "void yyrestart(FILE *f) { free(yybuffer); yybuffer = yycursor = yylimit = NULL; yyin = f; }\n"
        << "int yylex_destroy(void) { yyrestart(NULL); free(yytext); yytext = NULL; yytextcap = 0; yy_depth = 0; yy_start = INITIAL; return 0; }\n"
        << "static void yy_load(void) {\nsize_t used = 0, cap = 4096; int c;\n"
        << "if (!yyin) yyin = stdin;\nyybuffer = (char *)malloc(cap); if (!yybuffer) abort();\n"
        << "while ((c = fgetc(yyin)) != EOF) { if (used == cap) { char *p; cap *= 2; p = (char *)realloc(yybuffer, cap); if (!p) abort(); yybuffer = p; } yybuffer[used++] = (char)c; }\n"
        << "if (ferror(yyin)) { perror(\"RegexPPC input\"); exit(1); }\nyycursor = yybuffer; yylimit = yybuffer + used;\n}\n"
        << "int yylex(void) { if (!yybuffer) yy_load(); for (;;) {\n"
        << "if (yycursor == yylimit) { switch (yy_start) {\n";
    for (const auto& c : cfg.conditions) {
        out << "case " << c.first << ":\n";
        for (const auto& r : cfg.rules) if (r.eof && (r.conditions.empty() || r.conditions.count(c.first))) {
            out << "{" << r.action << "}\n"; break;
        }
        out << "return 0;\n";
    }
    out << "default: return 0; } }\nswitch (yy_start) {\n";
    for (const auto& c : cfg.conditions) {
        std::vector<std::string> patterns; std::vector<int> indices;
        for (std::size_t i = 0; i < cfg.rules.size(); ++i) {
            const auto& r = cfg.rules[i];
            if (!r.eof && (r.conditions.count(c.first) || (r.conditions.empty() && !c.second))) {
                patterns.push_back(r.pattern); indices.push_back(static_cast<int>(i));
            }
        }
        out << "case " << c.first << ": {\n";
        if (patterns.empty()) {
            if (cfg.nodefault) out << "fprintf(stderr, \"RegexPPC scanner jammed\\n\"); exit(1);\n";
            else out << "putchar((unsigned char)*yycursor++);\n";
        }
        else scanner(out, cfg, chomsky3::compile_lexer(patterns), prefix + "_" + c.first, indices, true);
        out << "} break;\n";
    }
    out << "default: return 0;\n}\n}\n}\n" << cfg.trailer;
    return out.str();
}
}
