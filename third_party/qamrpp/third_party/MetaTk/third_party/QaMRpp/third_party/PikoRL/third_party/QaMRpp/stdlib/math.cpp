#include "../QaMRpp.hpp"

namespace qamrpp {
namespace stdlib {

void load_math(Context& ctx) {
    (void)ctx.load_library_named("math");
}

} // namespace stdlib
} // namespace qamrpp
