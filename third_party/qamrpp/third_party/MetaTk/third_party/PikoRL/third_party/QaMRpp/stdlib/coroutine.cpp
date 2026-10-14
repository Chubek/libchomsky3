#include "../QaMRpp.hpp"

namespace qamrpp {
namespace stdlib {

void load_coroutine(Context& ctx) {
    (void)ctx.load_library_named("coroutine");
}

} // namespace stdlib
} // namespace qamrpp
