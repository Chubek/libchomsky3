#include "../QaMRpp.hpp"

namespace qamrpp {
namespace stdlib {

void load_utf8(Context& ctx) {
    (void)ctx.load_library_named("utf8");
}

} // namespace stdlib
} // namespace qamrpp
