#include "../QaMRpp.hpp"

namespace qamrpp {
namespace stdlib {

void load_debug(Context& ctx) {
    (void)ctx.load_library_named("debug");
}

} // namespace stdlib
} // namespace qamrpp
