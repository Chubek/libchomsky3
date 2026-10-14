#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto classify = [](std::string_view s) {
    if (dsl::pattern<"[0-9]+">::matches(s)) return "int";
    if (dsl::pattern<"[a-z]+">::matches(s)) return "word";
    return "other";
  };
  std::cout << classify("123") << " " << classify("abc") << "\n";
}
