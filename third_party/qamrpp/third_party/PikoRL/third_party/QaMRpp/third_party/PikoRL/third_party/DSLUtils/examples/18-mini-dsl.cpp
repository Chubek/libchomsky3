#include "DSLUtils.hpp"
#include <iostream>

struct MiniDSL : dsl::DSL<MiniDSL, dsl::Pipeline, dsl::CustomLiterals> {
  static constexpr auto literals = dsl::literal_set(
      dsl::lit<"_km">([](long double v) { return v * 1000.0L; }),
      dsl::lit<"_m">([](long double v) { return v; }));
};

int main() {
  auto meters = MiniDSL::parse_literal("2.5_km");
  auto shown = MiniDSL::wrap(meters) | dsl::pipe([](long double x) { return x + 10.0L; });
  std::cout << shown << "\n";
}
