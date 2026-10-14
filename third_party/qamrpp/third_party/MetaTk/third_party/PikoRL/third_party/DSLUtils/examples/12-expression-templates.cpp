#include "DSLUtils.hpp"
#include <iostream>

struct Scalar : dsl::DSL<Scalar, dsl::ExprTemplates> {
  double v{};
  Scalar() = default;
  explicit Scalar(double x) : v(x) {}
  double expr_value() const { return v; }
};

int main() {
  Scalar a{2}, b{3}, c{4};
  auto expr = a + b * c - a;
  std::cout << expr.eval() << "\n";
}
