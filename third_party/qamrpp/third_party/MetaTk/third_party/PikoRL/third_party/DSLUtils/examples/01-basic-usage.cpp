#include "DSLUtils.hpp"
#include <iostream>

struct BasicDSL : dsl::DSL<BasicDSL, dsl::Pipeline, dsl::Operators> {};

int main() {
  auto v = BasicDSL::wrap(5) | dsl::pipe([](int x) { return x + 3; })
           | dsl::pipe([](int x) { return x * 2; });
  auto gt5 = BasicDSL::make_pred([](int x) { return x > 5; });
  auto even = BasicDSL::make_pred([](int x) { return x % 2 == 0; });
  std::cout << v << " " << (gt5(v) && even(v)) << "\n";
}
