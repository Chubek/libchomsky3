#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto expr = dsl::node<"add">(dsl::leaf<"num">(1), dsl::leaf<"num">(2));
  std::cout << expr.dump() << "\n";
}
