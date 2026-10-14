#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto token_kind = [](std::string_view t) {
    if (dsl::pattern<"[0-9]+">::matches(t)) return "INT";
    if (dsl::pattern<"[a-z]+">::matches(t)) return "ID";
    return "UNK";
  };
  std::cout << token_kind("abc") << " " << token_kind("99") << "\n";
}
