#include <cstddef>
#include <cstdint>
#include <iostream>
#include <istream>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

int main() {
  std::string input((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
  std::vector<std::uint8_t> bytes(input.begin(), input.end());
  return LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
}
