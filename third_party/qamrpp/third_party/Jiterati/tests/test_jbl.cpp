/** @file tests/test_jbl.cpp
 *  @brief JBL round-trip and terse parser test.
 */
#include "Jiterati.hpp"
#include "Jiterati-Pass.hpp"

#include "../IR/IR.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        std::exit(1); \
    } \
} while (0)

int main() {
    jiterati::Module m;
    jiterati::Function* fn = m.create_function<int(int)>("id");
    jiterati::Block* entry = fn->create_block("entry");
    entry->ret(entry->arg(0));

    std::string jbl = jiterati::print_jbl(m);
    CHECK(std::strstr(jbl.c_str(), "func i32 @id") != nullptr);

    std::string error;
    auto parsed = jiterati::parse_jbl(jbl, &error);
    CHECK(parsed != nullptr);
    CHECK(parsed->find_function("id") != nullptr);

    std::vector<jiterati::ir::Diagnostic> diagnostics;
    auto terse = jiterati::ir::parse_terse_module(
        "terse module demo\n"
        "fn add(x: i64, y: i64) -> i64 = (+ x y)\n",
        &diagnostics);
    CHECK(terse.has_value());
    CHECK(diagnostics.empty());

    auto pipeline = jiterati::parse_jpl_pipeline("cfg; constant-folding; dce");
    pipeline.run(m);

    std::printf("test_jbl passed\n");
    return 0;
}
