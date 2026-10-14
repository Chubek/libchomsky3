#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto sign = dsl::optional(dsl::ch('-'));
  auto digit = dsl::satisfy([](char c) { return c >= '0' && c <= '9'; }, "digit");
  auto num = sign & *digit;
  auto out = dsl::run_parser(num, "-42");
  std::cout << (out.value.has_value() ? "parsed" : "failed") << "\n";
}
