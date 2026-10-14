#include "Klyspec.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const std::string input(reinterpret_cast<const char *>(data), size);
  auto result = klyspec::parse_klytmk(input);
  if (result.ok && !result.ast.has_value()) {
    std::cerr << "[azmatest] invalid parser state\n";
    return 1;
  }
  return 0;
}

int main() {
  const std::string seed =
      "command \"seed\" { }; param \"a/alpha=\" { help-string { x }; }; pre-evaluate { exec = \"/bin/echo\"; sanitize = \"stdlib/shell.sh\"; };";
  const auto *bytes = reinterpret_cast<const uint8_t *>(seed.data());
  (void)LLVMFuzzerTestOneInput(bytes, seed.size());
  std::cout << "[azmatest] fuzz harness seed run passed\n";
  return 0;
}
