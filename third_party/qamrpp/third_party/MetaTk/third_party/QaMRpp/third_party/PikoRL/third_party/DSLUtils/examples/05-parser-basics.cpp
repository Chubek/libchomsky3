#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto p = dsl::ch('a') & dsl::ch('b');
  auto out = dsl::run_parser(p, "ab");
  std::cout << (out.value.has_value() ? "ok" : "err") << "\n";
}
