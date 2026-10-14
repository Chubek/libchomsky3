#include "DSLUtils.hpp"
#include <iostream>

int main() {
  auto letter = dsl::satisfy([](char c) { return c >= 'a' && c <= 'z'; }, "letter");
  auto word = *letter;
  auto out = dsl::run_parser(word, "hello");
  std::cout << out.value->size() << "\n";
}
