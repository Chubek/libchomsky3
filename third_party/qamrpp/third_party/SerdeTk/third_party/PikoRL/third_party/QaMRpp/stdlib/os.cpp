#include "../QaMRpp.hpp"

namespace qamrpp {
namespace stdlib {

void load_os(Context& ctx) {
    (void)ctx.load_library_named("os");
}

} // namespace stdlib
} // namespace qamrpp
