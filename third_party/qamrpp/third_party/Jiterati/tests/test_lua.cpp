/** @file tests/test_lua.cpp
 *  @brief Lua macro bridge smoke tests.
 */
#include "Jiterati-Macro.hpp"

#include <cstdio>
#include <cstdlib>

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        std::exit(1); \
    } \
} while (0)

int main() {
    jiterati::LuaRuntime runtime;
    runtime.open_libraries();
    runtime.bind_api();
    runtime.run_string(R"lua(
        M = ljiterati.macro.new "QuickAdd"
        ran = false
        M.init = function() init_seen = true end
        M.main = function()
            ljiterati.insn.load(1)
            ljiterati.insn.load(2)
            ljiterati.insn.add()
            ljiterati.insn.ret()
            ran = true
        end
        M.cleanup = function() cleanup_seen = true end
        ljiterati.macro.run(M)
    )lua");

    CHECK(runtime.registry().has("QuickAdd"));
    CHECK(runtime.registry().size() == 1);
    CHECK(runtime.state()["ran"].get<bool>());
    CHECK(runtime.state()["init_seen"].get<bool>());
    CHECK(runtime.state()["cleanup_seen"].get<bool>());
    CHECK(runtime.state()["ljiterati"]["runtime"]["arch"].get<sol::function>()().valid());

    std::printf("test_lua passed\n");
    return 0;
}
