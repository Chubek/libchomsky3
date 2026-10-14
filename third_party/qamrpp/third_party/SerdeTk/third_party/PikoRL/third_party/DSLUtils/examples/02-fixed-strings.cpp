#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto n = dsl::leaf<"token">("identifier");
  auto t = dsl::node<"pair">(n, dsl::leaf<"kind">("name"));
  std::cout << t.tag() << " " << t.dump() << "\n";
}
