#ifndef REGEXPPC_RUNTIME_HPP
#define REGEXPPC_RUNTIME_HPP
#include <cstddef>
#include <cstdint>
#include <xsimd/xsimd.hpp>

namespace regexppc {
// Only complete batches inside [p, end) are loaded; the tail is scalar.
inline const unsigned char* skip_run(const unsigned char* p, const unsigned char* end,
                                      unsigned char lo, unsigned char hi) {
    using batch = xsimd::batch<std::uint8_t>;
    while (static_cast<std::size_t>(end - p) >= batch::size) {
        auto bytes = batch::load_unaligned(p);
        if (!xsimd::all((bytes >= batch(lo)) & (bytes <= batch(hi)))) break;
        p += batch::size;
    }
    while (p < end && *p >= lo && *p <= hi) ++p;
    return p;
}
}
#endif
