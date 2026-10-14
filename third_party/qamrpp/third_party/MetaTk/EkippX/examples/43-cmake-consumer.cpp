#include <EkippX/EkippX-Batteries.hpp>

#include <iostream>

int main() {
  auto ctx = ekippx::batteries::batteries_context();
  std::cout << ctx.expand_text("@emitln(&upper(consumer ok))");
  return 0;
}
