/** @file JIT.cpp
 *  @brief JIT driver implementation.
 */
#include "Jiterati.hpp"
#include "Jiterati-BE.hpp"

namespace jiterati {

JIT JIT::create_default() {
    return JIT{};
}

std::unique_ptr<CompiledFunction> JIT::compile(Function& fn, Backend& backend) {
    return backend.compile_function(fn);
}

} // namespace jiterati
