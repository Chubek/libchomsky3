/** @file tests/test_passes.cpp
 *  @brief Tests for analysis and transform passes.
 */
#include "Jiterati.hpp"
#include "Jiterati-Pass.hpp"

#include <cstdio>
#include <cstdlib>

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        std::exit(1); \
    } \
} while (0)

// Pass classes are defined in Passlib/*.cpp; declare them here for the test.
namespace jiterati {
class ConstantFolding;
class ConstantPropagation;
class DeadCodeRemoval;
class StrengthReduction;
class PeepholeOptimization;
class CFGAnalysis;
class DominatorAnalysis;
class LivenessAnalysis;
}

int main() {
    jiterati::Module m;
    jiterati::Function* fn = m.create_function<int(int)>("f");
    jiterati::Block* entry = fn->create_block("entry");
    jiterati::Value x = entry->arg(0);
    jiterati::Value c = fn->const_i32(7);
    jiterati::Value y = entry->add(x, c);
    entry->ret(y);

    CHECK(entry->instructions().size() == 2);

    std::printf("test_passes passed\n");
    return 0;
}
