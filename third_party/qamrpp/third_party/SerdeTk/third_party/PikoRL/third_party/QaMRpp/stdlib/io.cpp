#include "../QaMRpp.hpp"

namespace qamrpp {
namespace stdlib {

void load_io(Context& ctx) {
    (void)ctx.load_library_named("io");
}

} // namespace stdlib
} // namespace qamrpp
