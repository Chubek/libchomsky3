#include "../QaMRpp.hpp"

namespace qamrpp {
namespace stdlib {

void load_table(Context& ctx) {
    (void)ctx.load_library_named("table");
}

} // namespace stdlib
} // namespace qamrpp
