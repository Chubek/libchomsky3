#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto rules = dsl::rewrite_set(
      dsl::rule<"double-neg">(
          [](const dsl::ASTNode& n) { return !n.is_leaf() && n.tag() == "neg" && n.size() == 1 && !n.child(0).is_leaf() && n.child(0).tag() == "neg"; },
          [](const dsl::ASTNode& n) { return n.child(0).child(0); }));
  auto in = dsl::node<"neg">(dsl::node<"neg">(dsl::leaf<"num">(4)));
  std::cout << rules.apply(in).dump() << "\n";
}
