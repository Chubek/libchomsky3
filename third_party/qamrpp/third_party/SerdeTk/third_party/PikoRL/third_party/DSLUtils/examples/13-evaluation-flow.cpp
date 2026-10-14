#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto safe_div = [](double x, double y) -> dsl::Result<double, std::string> {
    if (y == 0.0) return dsl::Result<double, std::string>::from_err("division by zero");
    return dsl::Result<double, std::string>::from_ok(x / y);
  };
  auto r = safe_div(12, 3).map([](double v) { return v + 1; });
  std::cout << r.unwrap_or(-1) << "\n";
}
