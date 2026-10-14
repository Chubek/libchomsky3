#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto p = dsl::labeled(dsl::ch('x') & dsl::ch('y'), "xy");
  auto out = dsl::run_parser(p, "xz");
  if (!out.value) {
    std::cout << "error at " << out.error.pos << "\n";
  }
}
