#include "../QaMRpp.hpp"

namespace qamrpp {
namespace stdlib {

void load_package(Context& ctx) {
    (void)ctx.load_library_named("package");
}

} // namespace stdlib
} // namespace qamrpp
