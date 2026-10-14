#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto classify = [](int x) {
    if (x == 0) return "zero";
    if (x == 1) return "one";
    return "other";
  };
  std::cout << classify(1) << " " << classify(3) << "\n";
}
