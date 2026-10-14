/** @file tests/test_plugins.cpp
 *  @brief Plugin registry and built-in plugin helpers test.
 */
#include "Jiterati.hpp"
#include "Jiterati-Plugin.hpp"

#include <cstdlib>
#include <cstdio>

namespace jiterati {
std::size_t burs_default_pattern_count();
std::size_t max_munch_default_match_count();
}

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        std::exit(1); \
    } \
} while (0)

int main() {
    jiterati::PluginRegistry& reg = jiterati::PluginRegistry::instance();
    reg.register_plugin("test", { 0, 1, 0 });
    CHECK(reg.count() >= 1);
    CHECK(reg.has_plugin("test"));

    CHECK(jiterati::burs_default_pattern_count() == 3);
    CHECK(jiterati::max_munch_default_match_count() == 3);
    CHECK(reg.has_plugin("BURSISel"));
    CHECK(reg.has_plugin("MaxMunchISel"));
    std::printf("test_plugins passed\n");
    return 0;
}
