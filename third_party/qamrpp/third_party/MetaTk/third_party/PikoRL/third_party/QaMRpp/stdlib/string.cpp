#include "../QaMRpp.hpp"

namespace qamrpp {
namespace stdlib {

void load_string(Context& ctx) {
    (void)ctx.load_library_named("string");
}

} // namespace stdlib
} // namespace qamrpp
