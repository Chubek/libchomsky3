#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto expr = dsl::node<"add">(dsl::leaf<"num">(5), dsl::node<"mul">(dsl::leaf<"num">(2), dsl::leaf<"num">(3)));
  std::cout << expr.dump() << "\n";
}
