#include "../../DSLUtils.hpp"
#include <cstdio>
#include <string>

int main() {
  std::string data;
  char buf[4096];
  while (true) {
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    if (n == 0) break;
    data.append(buf, n);
  }

  auto alpha = dsl::satisfy([](char c){ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }, "alpha");
  auto digit = dsl::satisfy([](char c){ return c >= '0' && c <= '9'; }, "digit");
  auto ws = dsl::satisfy([](char c){ return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }, "ws");

  auto p = dsl::ch('(') | dsl::ch(')');
  auto out = dsl::run_parser(p, data);
  (void)out;
  return 0;
}
