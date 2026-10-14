#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto sq = dsl::memoize<int, int>([](int x) { return x * x; });
  dsl::Lazy<int> deferred([] { return 21 * 2; });
  std::cout << sq(12) << " " << sq(12) << " " << deferred.get() << "\n";
}
