/** @file tests/test_core.cpp
 *  @brief Core IR construction and JIT execution tests.
 */
#include "Jiterati.hpp"
#include "BE/AMD64.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        std::exit(1); \
    } \
} while (0)

int main() {
    jiterati::Module m("test");
    jiterati::Function* fn = m.create_function<int(int,int)>("add2");

    jiterati::Block* entry = fn->create_block("entry");
    jiterati::Value x = entry->arg(0);
    jiterati::Value y = entry->arg(1);
    jiterati::Value z = entry->add(x, y);
    entry->ret(z);

    CHECK(fn->param_count() == 2);
    CHECK(fn->entry_block() == entry);
    CHECK(entry->instructions().size() == 2);
    CHECK(entry->instructions()[0]->opcode() == jiterati::Opcode::Add);
    CHECK(entry->instructions()[1]->opcode() == jiterati::Opcode::Ret);

    std::string s = m.to_string();
    CHECK(std::strstr(s.c_str(), "func i32 @add2") != nullptr);
    CHECK(std::strstr(s.c_str(), "add %0 %1 : i32") != nullptr);

    // Compile and run.
    jiterati::be::AMD64Backend backend;
    jiterati::JIT jit = jiterati::JIT::create_default();
    auto compiled = jit.compile(*fn, backend);
    CHECK(compiled != nullptr);
    int (*add2)(int, int) = reinterpret_cast<int(*)(int, int)>(compiled->entry());
    CHECK(add2(3, 4) == 7);
    CHECK(add2(-10, 5) == -5);

    std::printf("test_core passed\n");
    return 0;
}
