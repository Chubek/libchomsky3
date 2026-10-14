#include "DSLUtils.hpp"
#include <iostream>

int count_nodes(const dsl::ASTNode& n) {
  int total = 1;
  for (const auto& c : n.children()) total += count_nodes(c);
  return total;
}

int main() {
  auto t = dsl::node<"mul">(dsl::leaf<"num">(3), dsl::node<"add">(dsl::leaf<"num">(1), dsl::leaf<"num">(2)));
  std::cout << count_nodes(t) << "\n";
}
