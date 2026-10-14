#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto rs = dsl::rewrite_set(
      dsl::rule<"add-zero-right">(
          [](const dsl::ASTNode& n) { return n.tag() == "add" && n.size() == 2 && n.child(1).tag() == "num" && n.child(1).value() == "0"; },
          [](const dsl::ASTNode& n) { return n.child(0); }),
      dsl::rule<"mul-one-right">(
          [](const dsl::ASTNode& n) { return n.tag() == "mul" && n.size() == 2 && n.child(1).tag() == "num" && n.child(1).value() == "1"; },
          [](const dsl::ASTNode& n) { return n.child(0); }));
  auto in = dsl::node<"mul">(dsl::node<"add">(dsl::leaf<"num">(7), dsl::leaf<"num">(0)), dsl::leaf<"num">(1));
  std::cout << rs.apply(in).dump() << "\n";
}
