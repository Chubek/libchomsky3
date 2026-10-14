#ifndef QAMRPP_STDPP_ALGORITHM_HPP
#define QAMRPP_STDPP_ALGORITHM_HPP

#include <algorithm>

namespace stdpp {
template <typename It>
void sort(It first, It last) {
    std::sort(first, last);
}
}

#endif
